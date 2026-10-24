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
- Commit: `6e1e2e6`.
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `18357f33503a36635d24b8d3185a40aa35052dd898cde3597eab0344b1f8801d`

## 2026-10-10 — Attempt software-only BOOTSEL recovery

- Step/experiment: `wp1-choupi-usb-reset-recovery`
- Change: Tried all available USB software reset paths against the running Pico app: `picotool reboot -f/-F -u`, `picotool load -F`, CDC 1200 baud, and direct libusb control transfers to the enumerated reset interface (including a claimed interface and class/interface request).
- Validation: Each tool path reported a BOOTSEL request or returned from the transfer, but `picotool info -a` still reported only CDC serial mode (exit 249). The USB device remained at bus 1, device 116. Keeping `/dev/ttyACM0` open during reset yielded zero bytes and did not change the device mode. `lsusb -t` shows CDC interfaces 0/1 and reset interface 2.
- Result: No software-only path reached BOOTSEL; the diagnostic image remains unflashed. The running VM emitted no further serial output after its IOException trace, consistent with firmware no longer servicing the reset path (inference from host observations).
- Decision: Keep the latest diagnostic candidate ready. Do not claim a reset or flash. Continue software-only source analysis; hardware flash remains unavailable until the device responds to a supported reset.
- Commit: `2ba182e`
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
- Commit: `48af2f1`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261011T093403Z.log` (SHA-256 `23c41afe5f954df2edf1b01eb26a5b90482a1288437d4f9dc30a1ffeb72702c9`); `artifacts/logs/ccid-probe-20261011.txt`; ARM UF2 SHA-256 `4f675fadf4913cf33d430494236c8853a6978a177873f9c5abe09b4a074eb995`

## 2026-10-11 — Audit APDU and applet lifecycle entry points

- Step/experiment: `wp3-apdu-lifecycle-entry-audit`
- Change: Traced the boot-time ROM-mask starter through the CHOUPI interpreter and Java Card API. Recorded the minimal missing path and checked the bundled GlobalPlatformPro host against PC/SC.
- Validation: Source inspection confirms firmware invokes one fixed static method at boot; non-static interpreter entry, `Applet.register()`, and `APDU.getBuffer()` are explicit TODOs, and no selected-applet registry exists. `./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar --list` exits with `SCARD_E_NO_SERVICE (0x8010001d)` because `pcscd` is absent. Inspection-only step; no build or flash was needed.
- Result: The existing static AlgTest ROM-mask runner cannot receive host APDUs, and GlobalPlatformPro cannot yet access a PC/SC resource manager. A CCID USB interface alone is insufficient for either GP package management or applet execution.
- Decision: Start WP3 by adding an APDU buffer and explicit applet selection/dispatch path, then provide a user-space PC/SC service/driver route before validating GPPro.
- Commit: nested API `f6776a8`; root regression/docs `3b16eae`.
- Artifacts/logs: `artifacts/logs/apdu-lifecycle-audit-20261011.txt`

## 2026-10-11 — Check for a user-local PC/SC service

- Step/experiment: `wp3-pcsc-user-local-daemon`
- Change: Downloaded `pcscd` 2.4.1-1 and `libccid` 1.7.1-1 to `/tmp`, extracted them without system installation, and attempted to start the PC/SC daemon with the extracted CCID driver.
- Validation: The daemon exited immediately with `cannot create /run/pcscd: Permission denied`. No daemon remains running, no system package was installed, and the Pico was not flashed during this experiment.
- Result: Direct libusb CCID probing works, but this account cannot start the system PC/SC daemon because its runtime socket directory is root-owned/absent. The repository rule forbids using sudo in an autonomous cycle.
- Decision: Keep the hardware transport verified via libusb; resume PC/SC/GPPro after the host exposes its normal PC/SC service or an unprivileged socket path is available. Continue firmware work on applet lifecycle and APDU dispatch.
- Commit: `19f4503`.
- Artifacts/logs: `artifacts/logs/pcsc-user-local-20261011.txt`; downloaded/extracted packages remain only under `/tmp/pico2-pcsc-root`.

## 2026-10-11 — Diagnose empty PC/SC scan after driver match

- Step/experiment: `wp3-pcsc-empty-reader-usb-state`
- Change: Checked PC/SC service state, USB enumeration, sysfs, and picotool after the aligned CCID driver match was added.
- Validation: pcscd socket and service were active, but `pcsc_scan -r` reported no reader. VM `lsusb` listed only root hubs and the VirtualBox USB Tablet; sysfs had no VID `2e8a`, and `picotool info -a` found no RP-series device. Inspection only; no build or flash was performed.
- Result: The reader was absent from the VM USB bus at the time of the scan, so libccid and pcscd had no device to enumerate.
- Decision: Reattach the Pico through VirtualBox; verify `lsusb -d 2e8a:0009`, then rerun `pcsc_scan -r`.
- Commit: `73efd4c`.
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
- Commit: `72a1ead` (this record's commit ID is recorded in the following
  metadata commit)
- Artifacts/logs: none

## 2026-10-11 — Add bounded CAP applet AID lookup

- Step/experiment: `wp5-cap-applet-aid-index`
- Change: Corrected `jc_cap_app::AID()` to use its one-byte AID length directly and added a bounded lookup that walks variable-length Applet component records, validates each 5–16 byte AID and record boundary, and compares the requested AID.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. `git diff --check` passed. Hardware/APDU validation was skipped because the current VM session reports no usable libusb context and no PC/SC service; no flash was attempted.
- Result: The VM has a safe CAP metadata primitive for locating applet entries by AID and reading the matching install method offset. No applet is selected or executed yet.
- Decision: Next wire the lookup into a persistent applet registry/lifecycle path; do not treat this parser primitive as SELECT or install support.
- Commit: `43a0721`
- Artifacts/logs: none

## 2026-10-11 — Record CAP lookup commit metadata

- Step/experiment: `wp5-cap-applet-aid-index-commit-metadata`
- Change: Recorded the implementation and source-file-mode commits for the bounded applet AID lookup step.
- Validation: `git ls-files -s choupi/src/jc_cap/jc_cap_applet.hpp` reports regular-file mode `100644`; both architecture builds passed for the implementation.
- Result: The implementation is committed as `52c7c7d`; the regular-file mode correction is committed as `6f54e5e`.
- Decision: Continue with firmware lifecycle integration.
- Commit: `8b9095d`; this commit records the validated step
- Artifacts/logs: none

## 2026-10-11 — Confirm v1.8.2 Applet component record layout

- Step/experiment: `wp5-cap-applet-aid-index-artifact-check`
- Change: No source change. Parsed the supplied CAP Applet component with a bounded Python inspection to verify the lookup's size accounting and variable-length record layout.
- Validation: `AlgTest_v1.8.2_jc305.cap` contains Applet.cap component size 14, count 1, AID `4A43416C675465737431`, and big-endian install offset `0x3DA3` (15779); record traversal consumes the component exactly with zero trailing bytes.
- Result: The new lookup's component-size convention matches the supplied JCAlgTest v1.8.2 Applet component.
- Decision: Continue into applet lifecycle dispatch. Host hardware access is unavailable in this VM session (`lsusb` reports libusb initialization error -99; system bus/PCSC unavailable), so defer flashing until USB access returns.
- Commit: nested API repository `2ec9efa`; root implementation commit pending.
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


## 2026-10-19 — Verify JCVMArray temporary cleanup when element copies throw

- Step/experiment: jcvmarray-element-copy-exception-safety
- Change: Extended the allocation regression with an element type whose copy
  assignment throws. Copy construction must free its partially populated
  backing array; copy assignment must free its temporary and preserve the
  destination's original length/value. The test build suppresses GCC's
  mismatched-new-delete diagnostic for the deliberate global allocation
  replacement backed by malloc/free.
- Validation: `./tools/test-jcvmarray-allocation` passed all six cases,
  including exact one-array deallocation checks for both throwing element-copy
  paths. Rebuild completed with no warnings.
- Result: The `unique_ptr` staging paths release allocations on element-copy
  exceptions, not only on allocation failure.
- Decision: Keep the exception-safety regression and continue the memory/API
  audit.
- Commit: `6285edf`
- Artifacts/logs: none.

## 2026-10-18 — Validate ARM smoke image on physical Pico 2

- Step/experiment: `physical-arm-smoke-cycle`
- Change: Flashed the ARM smoke UF2 to the attached Pico 2 and captured bounded
  serial output. The repository wrapper built both architectures as required
  by the flash gate; only ARM was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle arm` passed. ARM and
  RISC-V builds passed. The device lacked its normal reset interface, but the
  wrapper's CDC 1200-baud BOOTSEL recovery succeeded. Eight-second capture
  repeatedly reported `TEST:PASS random_stream_varies` and
  `TEST:SUMMARY pass=1 fail=0`; no `TEST:FAIL` or `FAULT` appeared.
- Result: Real-device ARM smoke validation passed. The board now runs the smoke
  image; this cycle did not exercise CHOUPI, the applet, or JCSystem.
- Decision: Resume applet-level physical testing only after reflashing CHOUPI.
  Continue the separate `makeGlobalArray()` design work.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-arm-20261018T074707Z.log` (SHA-256
  `eb58edba0a3c95145b4112453485896f959c2b4d2292e033340691b73ed222d3`).

## 2026-10-18 — Add physical Pico 2 JCAlgTest runbook

- Step/experiment: `readme-physical-jcalgtest-runbook`
- Change: Added root `readme.md` with concise commands to extract the official
  JCAlgTest v1.8.2 client, build/flash ARM CHOUPI, grant PC/SC access, install
  the supplied JC305 CAP if needed, and run `ALG_SUPPORT_EXTENDED` and
  `ALG_PERFORMANCE_STATIC` against the exact reader.
- Validation: Documentation-only review against `tools/cycle`,
  `tools/run-jcalgtest`, `tools/pico2-pcsc-reader-root`, and GlobalPlatformPro
  `--help`. No build, test, or hardware command was run.
- Result: The readme includes the existing 900-second client timeout and notes
  that `NO_SUCH_ALGORITHM` results are expected. It assumes the versioned
  client archive and local toolchain are already prepared.
- Decision: Use this as the short physical-device procedure; the current board
  still runs the ARM smoke image and must be reflashed with CHOUPI first.
- Commit: pending
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

## 2026-10-14 — Flash latest CHOUPI firmware from BOOTSEL

- Step/experiment: `bootsel-choupi-runtime-20261014`
- Change: Flashed the current ARM CHOUPI image to the physical RP2350 while it was already in BOOTSEL. No source changes were made.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed as the repository flash gate. `./tools/flash build/arm/pico2_choupi.uf2` detected BOOTSEL, reached 100%, and confirmed application reboot. `./tools/monitor --seconds 18` passed with 18 repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0` pairs; no `TEST:FAIL` or `FAULT` appeared. PC/SC validation did not run: `tools/probe-jc-algtest` reported the named reader missing. `pcsc_scan -r` found no service and emitted a rapid stream of `Service not available` / invalid-handle errors, so it was interrupted. No RISC-V firmware was flashed or tested.
- Result: The latest ARM firmware is running and its board self-check passes. This does not establish that an applet lifecycle method or APDU was executed.
- Decision: Resume JCAlgTest applet verification when a reader is visible and pcscd is responding; do not rerun `pcsc_scan -r` until service availability is confirmed.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/bootsel-choupi-runtime-20261014.log` (SHA-256 `724ba860bb056e2af06c72b77796d75bf88604b714398eaea9a046b345a02b30`); ARM UF2 SHA-256 `81fd209c5245c0c4f7977f561f0f3926575beb9168dff54113b380aad5163f03`.

## 2026-10-14 — BOOTSEL validation commit reference

- Step/experiment: `bootsel-choupi-runtime-20261014-reference`
- Change: Recorded the immutable commit for the latest physical ARM flash and runtime capture.
- Validation: Confirmed `3dbab14` contains the BOOTSEL flash record, handoff and TODO updates, and the 18-second serial artifact.
- Result: Hardware validation is committed as `3dbab14`; applet-level APDU validation remains blocked by unavailable PC/SC in this execution context.
- Decision: Continue with v1.8.2 applet APDU checks when the reader service is available.
- Commit: `3dbab14`
- Artifacts/logs: `artifacts/logs/bootsel-choupi-runtime-20261014.log`.

## 2026-10-14 — Validate JCSystem memory query arguments

- Step/experiment: `jc-system-available-memory-validation-20261014`
- Change: Corrected the buffer overload of `JCSystem.getAvailableMemory()` to distinguish null output (`NullPointerException`), negative/out-of-range offset (`ArrayIndexOutOfBoundsException`), and invalid memory type (`SystemException`). Added checks for all three valid memory types, output bounds, scalar and buffer invalid memory types, null, and invalid ranges.
- Validation: `./tools/test-jcarray` passed, including `TEST:PASS jni_available_memory_validation`; its host compile emitted existing packed-member and ignored-qualifier warnings. `./tools/build arm` passed with `BUILD:PASS arch=arm` and `BUILD:PASS target=choupi`; no RISC-V build/test was run. The image was not flashed because root `AGENTS.md` requires both architecture builds before flashing, while this run is explicitly ARM-only. `git diff --check` passed.
- Result: Native argument validation now reports the documented exception categories and only writes the two requested short words. This does not prove Java code can catch the native errors because C++ native-exception-to-Java-object mapping remains incomplete.
- Decision: Keep this native-layer correction and continue the Java exception bridge / remaining JCAlgTest-used API work. Hardware validation remains pending under the ARM-only build constraint.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/host-jcarray-available-memory-20261014.log` (SHA-256 `ec0d3d4879dfb1df265b73ea2f253a913898e469b68f0d890ffd1958b8fcbe53`); `artifacts/logs/arm-available-memory-validation-20261014.log` (SHA-256 `3b4ade2ece2d8f8b85082fb0f2b3ef1926d4e8689e5558cd92a058b3ea1f2599`); ARM UF2 SHA-256 `c561daabc403eaa757dced3b2ecf662dbb9477568458edc61b3985704d5468d9`.

## 2026-10-14 — JCSystem memory validation commit reference

- Step/experiment: `jc-system-available-memory-validation-20261014-reference`
- Change: Recorded the immutable commit for the native memory-query validation correction and its ARM-only evidence.
- Validation: Confirmed `fe2f25b` contains the native fix, focused cases, build/test logs, and tracker/handoff updates.
- Result: The validation correction is committed as `fe2f25b`; Java throwable dispatch and physical applet behavior remain unverified.
- Decision: Continue with Java exception mapping and other JCAlgTest-used runtime gaps.
- Commit: `fe2f25b`
- Artifacts/logs: `artifacts/logs/host-jcarray-available-memory-20261014.log`; `artifacts/logs/arm-available-memory-validation-20261014.log`.

## 2026-10-14 — Validate JCSystem memory arguments in Java

- Step/experiment: `jc-system-available-memory-java-guards-20261014`
- Change: Added Java-level null, offset/range, and memory-type checks to `JCSystem.getAvailableMemory(short[], short, byte)`. The wrapper now throws Java `NullPointerException` and `ArrayIndexOutOfBoundsException`, and calls `SystemException.throwIt(ILLEGAL_VALUE)` before reaching JNI. The nested Java Card API commit is `7caa650`.
- Validation: `./tools/build arm` passed API compilation, CAP conversion, ROM-mask generation, ARM image build, and CHOUPI image build. The modified framework CAP converter reported zero errors; existing warnings remain in other API packages for `int` support and missing previous-minor export files. No RISC-V build/test or flash was run. `git diff --check` passed.
- Result: This API's invalid-argument paths now raise Java throwables in bytecode, allowing existing `athrow` and CAP exception-handler dispatch to handle them. Hardware applet catch behavior remains unverified because PC/SC is unavailable and the image was not flashed.
- Decision: Keep the Java guards and continue implementing the remaining JCAlgTest API/runtime behavior; native C++ exception conversion for unrelated native calls remains an open gap.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/arm-jcsystem-java-validation-20261014.log` (SHA-256 `969b7e02fb89bf4c4a9d09355cce52b78e16d146b68ae038e15aa2ef9f904e32`); ARM UF2 SHA-256 `509d663da55b8d1becf53771f5ec3293e713a7fb9f1ee1cec31b96cff048cfb3`.

## 2026-10-14 — No-chaining APDU implementation hash reference

- Step/experiment: `apdu-no-chaining-le-bound-20261014-hash-reference`
- Change: Recorded the immutable commit containing the Le and T=1 no-chaining response bounds.
- Validation: Confirmed commit `7448bec` contains the `Context` bound, expanded APDU context tests, and TODO/handoff updates; the test wrapper and ARM build passed.
- Result: The APDU no-chaining fix is linked to commit `7448bec`; board-level APDU behavior remains pending.
- Decision: Continue implementing and validating the remaining applet transport behavior.
- Commit: `7448bec`
- Artifacts/logs: `/tmp/pico2-apdu-nochaining.log`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Reflash CHOUPI and verify runtime on Pico 2

- Step/experiment: `bootsel-choupi-reflash-cycle-20261014`
- Change: Rebuilt both RP2350 architecture targets and the CHOUPI target, then flashed the ARM CHOUPI UF2 to the connected Pico 2. The board initially appeared as running CHOUPI firmware with no picotool reset interface; the bounded CDC recovery entered BOOTSEL and the retry completed.
- Validation: `./tools/cycle choupi` passed both ARM and RISC-V build gates, flashed at 100%, and captured 18 seconds of serial output. The capture contains 18 `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0` pairs and no `TEST:FAIL` or `FAULT`. Bounded `timeout 5s pcsc_scan -r` reports `No reader found`.
- Result: The refreshed ARM firmware passes its board runtime check. Applet-level APDU validation remains blocked because the PC/SC reader is not visible after reboot.
- Decision: Continue with finite v1.8.2 APDU probing once PC/SC visibility is restored; no APDU was sent in this step.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261014T135618Z.log` (SHA-256 `f7fe9aaa9f77203d9a62053d96cf750ed4cd50d333215374919ea4336950939a`); ARM UF2 SHA-256 `509d663da55b8d1becf53771f5ec3293e713a7fb9f1ee1cec31b96cff048cfb3`.

## 2026-10-14 — Validation evidence commit reference

- Step/experiment: `arm-api-and-bootsel-cycle-evidence-reference-20261014`
- Change: Recorded the root documentation and validation artifacts for the preceding Java wrapper and Pico 2 runtime validation steps.
- Validation: Confirmed commit `b8c3805` contains the updated handoff/TODO, append-only records, ARM Java API build log, and successful CHOUPI cycle log.
- Result: Root evidence is committed as `b8c3805`; the physical ARM runtime check passes, while applet APDU validation remains pending because PC/SC reports no reader.
- Decision: Continue with applet-level APDUs when a PC/SC reader is visible.
- Commit: `b8c3805`
- Artifacts/logs: `artifacts/logs/arm-jcsystem-java-validation-20261014.log`; `artifacts/logs/cycle-choupi-20261014T135618Z.log`.
## 2026-10-14 — Add development task timer

- Step/experiment: `development-task-time-sample-20261014`
- Change: Added `tools/task-time` to append wall-clock intervals to `artifacts/metrics/development-time.csv`. It supports manual `start`/`stop`, timed commands, and a Markdown report; failed and interrupted attempts are counted but excluded from successful duration averages.
- Validation: Timer syntax check passed twice (0.055 s each). `./tools/task-time run compile --note 'ARM rebuild for current CHOUPI artifact' -- ./tools/build arm` passed in 3.903 s. `./tools/task-time run test --note 'short APDU host regression suite' -- ./tools/test-short-apdu` passed in 0.458 s. A manual analysis interval measured 4.641 s. Initial CLI parsing failed and is recorded as a failed attempt (11.839 s); parser was corrected. A requested finite board serial capture was interrupted after 82.460 s because this VM has no `/dev/ttyACM*` or `/dev/serial/by-id/*` node. Sysfs reports the Pico as `2e8a:0009` CHOUPI CCID, not BOOTSEL. No upload was attempted because the RISC-V artifact predates the latest shared-source edit and the root flash gate requires both targets to build; the active work is ARM-only.
- Result: Successful sample totals 9.112 s across compilation (3.903 s), focused testing (0.458 s), manual analysis (4.641 s), and timer syntax validation (0.110 s). The 3:29 Maven CAP-converter build completed earlier but began before the timer was added, so it is not in the sample. Upload time is unmeasured.
- Decision: Keep the timer append-only and use it across a longer development interval before drawing firm optimization conclusions. First improvements to measure are cached CAP conversion and earlier serial-node detection. Repeat compile/test measurements over several edits; record upload only after both-target build gate and BOOTSEL visibility are restored.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Development timer commit reference

- Step/experiment: `development-task-time-sample-20261014-reference`
- Change: Recorded the immutable commit for the task timer, usage documentation, and initial measurement sample.
- Validation: `tools/task-time` syntax validation passed twice; the ARM build and short APDU host test completed successfully. `git diff --cached --check` passed before commit.
- Result: The timer step is committed as `eb36cbd`; its report contains 9.112 seconds of successful recorded work. Upload remains unmeasured.
- Decision: Continue timing compile, analysis, upload, and test intervals across future implementation steps; hardware access remains unavailable from this VM device-node view.
- Commit: `eb36cbd`
- Artifacts/logs: `artifacts/metrics/development-time.csv` (SHA-256 `84a5be49f87005eaab8d03321c10bd9bd491bf910405d8e9aabff2b60fa9c243`).

## 2026-10-14 — Document development performance tips

- Step/experiment: `development-performance-tips-20261014`
- Change: Added `perf_tips.md` with the timer's current successful task timings, the unmeasured Maven observation, and practical suggestions for incremental builds, bounded parallelism, and faster USB/serial diagnosis.
- Validation: Reviewed the document against `artifacts/metrics/development-time.csv`, the recorded ARM build/test output, and the current USB/serial visibility inspection. `git diff --check` passed. No code tests were run because this step changes documentation only.
- Result: The recommendations distinguish measured values from the small sample's limitations and identify upload time as not measured.
- Decision: Gather more task-time samples during subsequent implementation before making performance changes.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; measurement source `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Performance tips commit reference

- Step/experiment: `development-performance-tips-20261014-reference`
- Change: Recorded the immutable commit containing the performance recommendations and their progress entry.
- Validation: Confirmed commit `6154378` contains only `perf_tips.md`, this progress reference, and the corresponding handoff update; staged whitespace check passed.
- Result: Performance guidance is committed as `6154378`.
- Decision: Continue the remaining JCAlgTest runtime and integration work; measure any future optimization before claiming a speedup.
- Commit: `6154378`
- Artifacts/logs: `perf_tips.md`; source measurements in `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Add rollback support for RP2350 persistent install writes

- Step/experiment: `persistent-install-rollback-rp2350-20261014`
- Change: Added RP2350 filesystem erase tombstones so rollback can hide transaction-created overlay entries and safely reuse their index slots. Persistent write snapshots now also cover indexed array and package-list updates. The install transaction still rolls back records when Java install or instance metadata registration fails.
- Validation: `./tools/test-persistent-transaction` passed with `TEST:SUMMARY fail=0` in 2.789 s. `./tools/build arm` passed in 3.639 s and linked `build/arm/pico2_choupi.uf2`. The earlier build attempt failed at link time because `fs_erase` was missing; the RP2350 implementation resolved it. No RISC-V build or hardware flash was performed.
- Result: Synchronous rollback now covers full-record writes and indexed writes in the CHOUPI filesystem overlay. The deletion path is build-validated but was not exercised on hardware. Existing host-test warnings about upstream const-qualified returns and packed CAP members remain.
- Decision: Keep installation atomicity open until failure-path hardware validation and useful status-word behavior are verified. CAP metadata and filesystem writes do not yet share a durable power-loss journal.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`; ARM UF2 `build/arm/pico2_choupi.uf2`; no archived build log.

## 2026-10-14 — Install failure status-word commit reference

- Step/experiment: `install-failure-status-map-20261014-reference`
- Change: Recorded the immutable commit for GP INSTALL failure-stage status mapping and its focused host test.
- Validation: Confirmed commit `7c71176` contains the status mapping, all ten host mapping cases, and the progress record. The focused test and ARM build passed before commit; staged whitespace validation passed.
- Result: The status mapping step is committed as `7c71176`. Hardware INSTALL response behavior remains unverified.
- Decision: Continue with host-side applet execution coverage while retaining hardware acceptance as open.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Install rollback commit reference

- Step/experiment: `persistent-install-rollback-rp2350-20261014-reference`
- Change: Recorded the immutable implementation commit for synchronous failed-install rollback and RP2350 overlay erase support.
- Validation: Confirmed commit `4e2d1a9` includes the transaction test, ARM filesystem implementation, GP INSTALL wrapper, and progress record. The focused test and ARM build passed before commit; staged whitespace validation passed.
- Result: The implementation step is committed as `4e2d1a9`. Hardware failure-path behavior remains untested.
- Decision: Continue with the remaining JCAlgTest gaps, retaining the power-loss atomicity and useful status-word work as open.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`; ARM UF2 `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Map install failures to actionable status words

- Step/experiment: `install-failure-status-map-20261014`
- Change: Mapped GP INSTALL failure stages to distinct status words: malformed data/applet errors `6A80`, missing package `6A82`, storage/rollback failure `6581`, transaction unavailable `6985`, and unexpected exception `6F00`. Existing diagnostic response bytes remain available before the final status word.
- Validation: `./tools/test-install-status` passed all ten mapping cases with `TEST:SUMMARY fail=0` in 1.551 s. `./tools/build arm` passed in 3.120 s. No RISC-V build or hardware flash was performed.
- Result: Host code and the ARM image agree on useful INSTALL status words. The actual card-manager APDU response has not been tested on hardware.
- Decision: Keep rollback/status validation open until an authorized board cycle is available; continue toward process APDU coverage while preserving the no-flash gate.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`; ARM UF2 `build/arm/pico2_choupi.uf2`; no archived build log.

## 2026-10-14 — Expand development performance measurements

- Step/experiment: `development-performance-tips-sample-expansion-20261014`
- Change: Updated `perf_tips.md` with four ARM build samples and six focused host-test samples, including means, ranges, non-OK counts, and guidance to separate clean and incremental runs.
- Validation: Cross-checked the table and the 31.169-second successful total against `./tools/task-time report` and the append-only CSV. `git diff --check` passed. Documentation only; no code tests were run.
- Result: ARM builds measured 3.120–5.819 s (mean 4.120 s); host tests measured 0.458–2.789 s (mean 1.656 s). Upload remains unmeasured, and the bounded card probe found no PC/SC reader.
- Decision: Continue collecting comparable clean/incremental and hardware timing samples before recommending build-system changes.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Record performance-guidance commit

- Step/experiment: `development-performance-suggestions-reference-20261015`
- Change: Recorded the immutable performance-guidance commit in the progress
  log and session handoff.
- Validation: `git diff --check -- progress.md .agent/HANDOFF.md` passed;
  documentation commit `be0a0e9` contains the timing summary and suggestions.
- Result: `perf_tips.md` now reflects the current timer report and clearly marks
  sample limitations and upload as unmeasured.
- Decision: Collect more comparable timing data during later build and hardware
  work; do not tune from this small observational sample.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Performance sample update commit reference

- Step/experiment: `development-performance-tips-sample-expansion-20261014-reference`
- Change: Recorded the immutable commit containing updated `perf_tips.md`, measurements, and handoff notes.
- Validation: Confirmed commit `171d404` contains the expanded timing table and suggestions; documentation whitespace validation passed.
- Result: The performance guidance update is committed as `171d404`.
- Decision: Add future timer samples after comparable clean and incremental runs; upload remains unmeasured.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Test RP2350 overlay erase and collision handling

- Step/experiment: `rp2350-fs-overlay-erase-regression-20261014`
- Change: Added a host test that compiles the actual RP2350 filesystem backend with hardware stubs and covers erase visibility, same-tag reuse, collision-chain lookups, and compressed child-tag reuse. The regression exposed stale data offsets after deleting a colliding entry; compaction now adjusts every occupied hash slot rather than only the first `overlay_file_count` slots.
- Validation: The initial host build needed additional Pico time API stubs. The first behavioral run failed `collision_probe_survives_erase`, which led to the offset fix. Final standalone `./tools/test-rp2350-fs-overlay` passed 13 checks with `TEST:SUMMARY fail=0` in 4.039 s; an earlier full compile/test pass took 5.184 s. `./tools/build arm` passed in 9.152 s. No RISC-V build or hardware flash was performed.
- Result: Erase, reuse, probing, and compressed-tag behavior pass against the target backend code. Existing upstream const-return and packed-member warnings are suppressed in the host test target; the ARM build passed without new warnings.
- Decision: Keep board failure-path verification open, then continue the next host-side JCAlgTest integration gap.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `tools/test-rp2350-fs-overlay`; ARM UF2 `build/arm/pico2_choupi.uf2`; timer data `artifacts/metrics/development-time.csv`.

## 2026-10-14 — RP2350 overlay regression commit reference

- Step/experiment: `rp2350-fs-overlay-erase-regression-20261014-reference`
- Change: Recorded the immutable commit for the RP2350 overlay erase test and collision-offset fix.
- Validation: Confirmed commit `c8fc232` contains the filesystem fix, standalone host regression, and progress record. The 13-case host test and ARM build passed before commit; staged whitespace validation passed.
- Result: The overlay erase regression step is committed as `c8fc232`; hardware validation remains outstanding.
- Decision: Continue with updated performance measurements and the remaining host-side JCAlgTest integration gaps.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `tools/test-rp2350-fs-overlay`; `build/arm/pico2_choupi.uf2`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Update performance tips with filesystem test timings

- Step/experiment: `development-performance-tips-fs-test-20261014`
- Change: Updated `perf_tips.md` and committed the accumulated raw timer rows. The report now covers five successful ARM builds and nine successful host tests.
- Validation: `./tools/task-time report` reports 52.821 s successful total; ARM builds 3.120–9.152 s (mean 5.127 s), host tests 0.458–5.184 s (mean 2.493 s). `git diff --check` passed. Documentation only; no code tests were run in this step.
- Result: The performance tips now reflect rollback, install-status, and actual RP2350 filesystem tests. Upload remains unmeasured.
- Decision: Continue timing comparable runs and proceed with host-side lifecycle/client coverage.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Filesystem timing update commit reference

- Step/experiment: `development-performance-tips-fs-test-20261014-reference`
- Change: Recorded the immutable commit containing updated performance tips and accumulated task-time rows.
- Validation: Confirmed commit `d27321d` includes the expanded timing summary, append-only CSV rows, and progress record; staged whitespace validation passed.
- Result: The updated performance summary is committed as `d27321d`.
- Decision: Continue with host-side applet lifecycle/client coverage; no PC/SC reader is currently visible.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Add next performance measurement suggestions

- Step/experiment: development-performance-next-measurements-20261014
- Change: Extended `perf_tips.md` with the current two-job build default rationale and a controlled plan for comparing clean/incremental builds, CPU use, hardware preflight, and CAP conversion.
- Validation: `./tools/task-time report` still reports 5 successful ARM builds (3.120–9.152 s, mean 5.127 s), 9 successful host tests (0.458–5.184 s, mean 2.493 s), and 52.821 s total. `git diff --check` passed. Documentation only; no tests or builds were run.
- Result: Suggestions distinguish measured timing from open experiments; upload and CPU-load comparisons remain unmeasured.
- Decision: Keep two build jobs as the interactive default until comparable CPU and wall-time samples support a change.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Record performance guidance commit reference

- Step/experiment: development-performance-next-measurements-20261014-reference
- Change: Recorded the immutable commit containing the updated performance suggestions and associated handoff/progress notes.
- Validation: Confirmed commit `49f848d` contains `perf_tips.md`, `progress.md`, and `.agent/HANDOFF.md`; `git diff --check` passed for the follow-up reference.
- Result: The requested performance guidance is committed as `49f848d`.
- Decision: Collect comparable build and hardware timing samples before changing the two-job default or build dependencies.
- Commit: `49f848d`
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Implement applet identity for JCSystem.getAID

- Step/experiment: jcsystem-get-aid-context-20261014
- Change: VM contexts now retain the applet instance package and AID. INSTALL initializes that identity before Java execution; applet restore initializes it from the registered instance AID. The native `JCSystem.getAID()` resolves the JC 3.0.5 framework AID class through the applet CAP import metadata, builds its `theAID` byte array, and caches the Java object for the context.
- Validation: `./tools/test-jcarray` passed, including new valid/invalid AID length and context retention checks. `./tools/build arm` passed and linked the RP2350 ARM CHOUPI image. `git -c core.whitespace=cr-at-eol diff --check` passed. No RISC-V build/test or board flash was performed; Java-level getAID execution remains unverified.
- Result: ARM build contains the native implementation and both install/restore paths provide instance identity. Existing host test emits upstream CAP packed-member and ignored-qualifier warnings.
- Decision: Continue with the next JCAlgTest API/runtime gap; retain applet-level identity verification as open until a permitted integration path is available.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `build/arm/pico2_choupi.uf2`; `tools/test-jcarray`.

## 2026-10-14 — Record JCSystem AID implementation commit

- Step/experiment: jcsystem-get-aid-context-20261014-reference
- Change: Recorded the implementation commit for `JCSystem.getAID()` instance identity and native object construction.
- Validation: Confirmed commit `cd58def` contains the context, install/restore plumbing, JNI implementation, focused test, TODO update, and original progress entry.
- Result: The implementation is committed as `cd58def`; direct Java-level execution remains open.
- Decision: Continue with the remaining JCAlgTest API surface and host APDU fixtures.
- Commit: `cd58def`
- Artifacts/logs: `build/arm/pico2_choupi.uf2`; `tools/test-jcarray`.

## 2026-10-14 — Avoid repeated PC API and ROM-mask regeneration

- Step/experiment: pc-build-incremental-generation-20261014
- Change: Replaced always-run CMake Java Card API and ROM-mask targets with output-stamp commands depending on their source inputs. Updated `perf_tips.md` and `TODO.md` with measured build costs and the optimization.
- Validation: Before the change, a full PC build passed but spent 1:01 regenerating the API and 5:13 in the Maven capmap reactor (including blocked-DNS metadata retries). After verifying `generation.sha256` and the current API source digest, bootstrapped stamps from those completed artifacts. `./tools/build-pc` then reported `ROMMASK:PASS` without rerunning Java/Maven, and `./tools/env cmake --build build/choupi-pc -j2` passed with `Built target choupi` while skipping generation. `git diff --check` passed. No ARM/RISC-V target build or hardware test was run in this CMake-only step.
- Result: Repeated PC CMake builds no longer clean/reconvert the Java API or invoke the Maven ROM-mask reactor when tracked inputs are unchanged. Full clean regeneration still takes several minutes and remains unoptimized.
- Decision: Keep source globs as invalidation inputs; next resume APDU host transport and applet lifecycle verification.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `build/choupi-pc/generation.sha256`; `build/choupi-pc/flash`; `build/choupi-pc/jni.hpp`.

## 2026-10-14 — Record PC incremental-generation commit

- Step/experiment: pc-build-incremental-generation-20261014-reference
- Change: Recorded the CMake generation-stamp commit and the associated current handoff reference.
- Validation: Confirmed commit `0da5b1d` contains the CMake stamp targets, performance measurements, TODO update, and original progress record.
- Result: The incremental PC build change is committed as `0da5b1d`.
- Decision: Resume the remaining APDU transport and applet lifecycle work.
- Commit: `0da5b1d`
- Artifacts/logs: `build/choupi-pc/generation.sha256`; `build/choupi-pc/flash`; `build/choupi-pc/jni.hpp`.

## 2026-10-14 — Add VM responsiveness and measurement suggestions

- Step/experiment: `performance-guidance-vm-responsiveness-20261014`
- Change: Added recommendations to diagnose the reported eight-core VM saturation, use bounded USB/PCSC readiness checks, and record CPU/resource metadata with comparable build and hardware timing stages. Updated the handoff with the documentation outcome.
- Validation: `git diff --check -- perf_tips.md .agent/HANDOFF.md progress.md` passed. Documentation-only change; no builds or tests were run.
- Result: Performance guidance separates the observed VM freeze symptom from its unmeasured cause and gives concrete next measurements.
- Decision: Collect process/thread CPU time during a reproducible freeze before tuning concurrency or attributing the load to a wait loop.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; no new timing data collected.

## 2026-10-14 — Record performance suggestions commit

- Step/experiment: `performance-guidance-vm-responsiveness-20261014-reference`
- Change: Recorded the immutable commit containing the expanded performance suggestions and handoff update.
- Validation: Confirmed commit `cd88f95` contains `perf_tips.md`, `.agent/HANDOFF.md`, and the original progress entry; whitespace validation passed.
- Result: Performance guidance is committed as `cd88f95`.
- Decision: Continue with host applet lifecycle/APDU coverage; measure CPU use if VM saturation recurs.
- Commit: `cd88f95`
- Artifacts/logs: `perf_tips.md`; no new timing data collected.

## 2026-10-14 — Implement Java Card CLA helpers

- Step/experiment: `javacard-apdu-cla-helpers-20261014`
- Change: Implemented CLA channel, chaining, secure-messaging, interindustry, and validity helpers in `APDUUtil`; wired current-command methods in `APDU`. Fixed Ant's `-Dbuild.dir` argument quoting so changed API sources regenerate successfully. Updated the TODO, performance note, and handoff.
- Validation: `./tools/test-apdu-util` passed 17 checks. Initial `./tools/build arm` failed because Ant received literal quotes in the output path; after the CMake fix, `./tools/build arm` passed API compilation/CAP conversion, ROM-mask generation, and ARM firmware build. Existing converter/Maven warnings remain. CRLF-aware whitespace checks passed. No RISC-V build, flash, or physical applet invocation was performed.
- Result: ARM UF2 SHA-256 `e6770c1145b65b9ff2fa4d5aadf32dfeda1aac5e236209158e02453b06dedbce`. Applet-level behavior is not yet verified. A `pcsc_scan -r` attempt with pcscd unavailable emitted over 237 MB of repeated service-unavailable output; it was stopped and documented as unsafe before pcscd readiness.
- Decision: Continue with the host vpcd APDU path and lifecycle validation; do not rerun `pcsc_scan` unless the daemon is confirmed ready.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `build/arm/pico2_choupi.uf2`; nested API commit `4d996f4`; `tools/test-apdu-util`.

## 2026-10-14 — Add vpcd frame codec

- Step/experiment: `vpcd-frame-codec-20261014`
- Change: Added a PC-only vpcd frame reader/writer using blocking socket I/O, partial-transfer loops, 16-bit network-order length framing, and explicit close, malformed-frame, and I/O outcomes. Added a standalone codec test and updated the PC-vpcd TODO/handoff.
- Validation: `./tools/test-vpcd-protocol` passed 14 checks. `./tools/env cmake --build build/choupi-pc --target choupi -j2` passed and compiled the PC socket implementation. `./tools/build arm` passed. Socket send/receive could not be exercised because this sandbox denies `send()` with `EPERM`; no RISC-V build or flash was run.
- Result: Protocol encoding/decoding covers control and APDU frames, malformed lengths, truncation, trailing bytes, and invalid payload inputs. Host connection and VM APDU dispatch remain to be integrated.
- Decision: Continue by wiring the codec into the PC emulator and determine a reproducible way to install/restore a test applet in its persistent filesystem.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `tools/test-vpcd-protocol`; `build/choupi-pc/choupi`; `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Record vpcd codec commit

- Step/experiment: `vpcd-frame-codec-20261014-reference`
- Change: Recorded the immutable commit for the PC vpcd frame codec and focused test.
- Validation: Confirmed commit `f832200` contains the codec, test wrapper, and project tracking update; `git diff --check` passed.
- Result: The vpcd frame codec is committed as `f832200`.
- Decision: Continue with host connection/control handling and applet lifecycle dispatch.
- Commit: `f832200`
- Artifacts/logs: `tools/test-vpcd-protocol`; `build/choupi-pc/choupi`; `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Record APDU helper implementation commit

- Step/experiment: `javacard-apdu-cla-helpers-20261014-reference`
- Change: Recorded the root commit for the APDU CLA API, test harness, regeneration fix, and handoff updates.
- Validation: Confirmed commit `c2935e7` contains the root implementation/test/docs files; nested Java API source commit is `4d996f4`. `git diff --check` and the CRLF-aware nested API check passed.
- Result: APDU CLA helper implementation is recorded in root commit `c2935e7`.
- Decision: Resume the remaining host vpcd transport and applet lifecycle validation.
- Commit: `c2935e7`
- Artifacts/logs: `build/arm/pico2_choupi.uf2`; nested API commit `4d996f4`; `tools/test-apdu-util`.

## 2026-10-14 — Integrate PC vpcd session and APDU routing

- Step/experiment: `pc-vpcd-session-apdu-routing-20261014`
- Change: Added a unit-testable PC vpcd session state machine and wired VPCD connection messages, ATR, applet lifecycle, and APDUs into the VM bridge. Enabled CMake source glob reconfiguration for newly added PC C++ files. Updated TODO and handoff with the remaining applet execution and transport gaps.
- Validation: `./tools/test-vpcd-protocol` passed 14 checks; `./tools/test-vpcd-session` passed 14 checks. `./tools/build-pc` and the PC `choupi` target passed; `build/choupi-pc/choupi --help` lists the new options. `./tools/build arm` passed. No RISC-V build/test or hardware flash was run. `./tools/probe-jc-algtest` found no PC/SC reader and sent no APDUs.
- Result: ARM UF2 SHA-256 `b98b999a8837858ef655453db0dab1ec9ffa9e30d8513901980a54295ae41625`; PC executable SHA-256 `38559217a0eb1dd8750f8f6c3dc915d2c727551916d790f17b25eb4168e98ee4`. Session tests use fake VM callbacks; live vpcd/TCP and installed JCAlgTest process behavior remain unverified.
- Decision: Continue with golden Java applet execution coverage and seek a host transport path that can exercise the PC emulator; use a bounded reader probe before hardware APDUs.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `tools/test-vpcd-protocol`, `tools/test-vpcd-session`, `build/arm/pico2_choupi.uf2`, `build/choupi-pc/choupi`; temporary build logs under `/tmp/pico2-vpcd-*.log`.

## 2026-10-14 — Record PC vpcd session integration commit

- Step/experiment: `pc-vpcd-session-apdu-routing-20261014-reference`
- Change: Recorded the immutable implementation commit for the PC vpcd session, lifecycle routing, focused test, and project tracking updates.
- Validation: Confirmed commit `ef1102d` contains the session implementation, integration, tests, TODO, and handoff changes; `git diff --check` passed.
- Result: The vpcd session integration is committed as `ef1102d`.
- Decision: Continue toward real applet execution evidence; fake callback tests do not satisfy JCAlgTest acceptance criteria.
- Commit: recorded in the current commit.
- Artifacts/logs: `tools/test-vpcd-protocol`, `tools/test-vpcd-session`, ARM UF2 and PC executable hashes in the preceding progress entry.

## 2026-10-14 — Add practical performance profiling workflow

- Step/experiment: `performance-profiling-guidance-20261014`
- Change: Added concrete process/thread inspection commands and recommendations to compare wall and CPU time, avoid competing builds, bound device discovery retries, and choose optimizations from repeatable profiles.
- Validation: `git diff --check -- perf_tips.md progress.md` passed. Documentation-only change; no builds, tests, or hardware operations were run.
- Result: The suggestions provide a way to identify the source of VM saturation without attributing it to an unmeasured cause.
- Decision: Capture process and timing data during any repeatable slowdown before changing concurrency or wait behavior.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; no new timing data collected.

## 2026-10-14 — Record performance profiling guidance commit

- Step/experiment: `performance-profiling-guidance-20261014-reference`
- Change: Recorded the immutable commit for the performance profiling recommendations.
- Validation: Confirmed commit `774b698` contains `perf_tips.md` and the original progress record; `git diff --check` passed.
- Result: The profiling workflow is committed as `774b698`.
- Decision: Use the listed CPU, wall-time, and resource measurements on the next reproducible slowdown.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; no new timing data collected.

## 2026-10-14 — Refresh performance recommendations with latest timings

- Step/experiment: `performance-tips-latest-host-sample-20261014`
- Change: Added the measured host applet installation cost (0.45 s wall, 0.40 s user CPU, 6,120 KiB peak RSS) and the 34.837-second failed PC smoke run to `perf_tips.md`. Clarified that the install sample is too small for optimization decisions and prioritized phase timing for the failed smoke path.
- Validation: `git diff --check -- perf_tips.md progress.md .agent/HANDOFF.md` passed. Documentation-only update; no build, test, or hardware operation was run.
- Result: The recommendations now separate a measured fast install phase from an unexplained long failed smoke run; its time is not attributed to applet runtime without phase evidence.
- Decision: Add emulator/VPCD/APDU phase timestamps on a future smoke run before optimizing it.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/host-install-rommask-20261014.log`; `artifacts/metrics/development-time.csv`; `perf_tips.md`.

## 2026-10-14 — Record performance recommendations commit

- Step/experiment: `performance-tips-latest-host-sample-20261014-reference`
- Change: Recorded the immutable documentation commit for the latest performance measurements and recommendations.
- Validation: `git diff --check -- progress.md` passed; commit `b5b6307` contains the documentation update.
- Result: The performance guidance update is committed as `b5b6307`.
- Decision: Add phase timing to the next PC smoke run before attempting targeted performance changes.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/logs/host-install-rommask-20261014.log`; `artifacts/metrics/development-time.csv`.

## 2026-10-14 — Correct CAP package-method resolution

- Step/experiment: `cap-package-method-lookup-20261014`
- Change: Masked the package-method flag from the package table token, fixed the `uint16_t` element offset used to locate the package virtual-method table, and made interpreter entry setup convert caught exceptions to `6F00` rather than terminating the host process. Updated TODO and handoff with live VPCD evidence.
- Validation: `./tools/env cmake --build build/choupi-pc --target choupi -j2` passed; `./tools/build arm` passed. `timeout 45s ./tools/test-pc-vpcd-applet` remains failing at the next runtime issue: ATR and SELECT pass, JCSystem info returns `6F00` after applet `process(APDU)` reaches CP field reference 174 and throws `SecurityException`. No RISC-V build or hardware flash was run.
- Result: The bad package method offset (`0xFF00`) is resolved; CAP token `0x81` now resolves to method offset `15960` and executes. Applet APDU processing is still incomplete.
- Decision: Inspect CP field reference 174 and the receiver field token/type, then continue the process APDU path.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/host-vpcd-jcalgtest-20261014.log`; `build/choupi-pc/choupi`; `build/arm/pico2_choupi.uf2`.

## 2026-10-14 — Record CAP package-method resolution commit

- Step/experiment: `cap-package-method-lookup-20261014-reference`
- Change: Recorded the immutable implementation commit for the package-method token, table-pointer, and VM-entry exception fixes.
- Validation: `git -c core.whitespace=cr-at-eol diff --check -- progress.md` passed; commit `daf56ab` contains the implementation and step record.
- Result: The resolver correction is committed as `daf56ab`.
- Decision: Continue with the still-failing applet field access at CP entry 174.
- Commit: recorded in the current commit.
- Artifacts/logs: VPCD integration log `artifacts/logs/host-vpcd-jcalgtest-20261014.log`; ARM and PC build outputs.

## 2026-10-14 — Add VPCD applet-debug performance suggestions

- Step/experiment: `performance-tips-vpcd-applet-debug-20261014`
- Change: Added targeted guidance to `perf_tips.md` for timing VPCD/APDU phases, bounding response/test/cleanup deadlines, checking CPU versus wait time, limiting verbose traces, and keeping a one-APDU reproducer while resolving CP entry 174.
- Validation: `git diff --check -- perf_tips.md progress.md` passed. Documentation-only change; no build, test, or hardware operation was run.
- Result: Suggestions apply to the observed `6F00` applet field-access failure and the integration run that exceeded its expected bounded duration; no performance cause is claimed as measured.
- Decision: Instrument the next VPCD run by phase and process CPU time before tuning the emulator or APDU path.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/logs/host-vpcd-jcalgtest-20261014.log`.

## 2026-10-14 — Record VPCD performance-tips commit

- Step/experiment: `performance-tips-vpcd-applet-debug-20261014-reference`
- Change: Recorded the commit reference for the VPCD applet-debug performance suggestions.
- Validation: `git diff --check -- perf_tips.md progress.md` passed; commit `447c88f` contains the recommendations and their progress record.
- Result: The targeted performance guidance is committed as `447c88f`.
- Decision: Use per-phase timing and bounded process cleanup on the next VPCD integration attempt.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/logs/host-vpcd-jcalgtest-20261014.log`.

## 2026-10-14 — Bound VPCD integration-test emulator cleanup

- Step/experiment: `vpcd-test-process-group-cleanup-20261014`
- Change: Updated the PC VPCD integration harness to start the emulator in a separate process group and stop/reap it with bounded SIGINT, SIGTERM, and SIGKILL escalation. Updated TODO and handoff with the latest field-layout evidence and remaining APDU failure.
- Validation: `./tools/env cmake --build build/choupi-pc --target choupi -j2` passed; Python AST syntax inspection passed. The bounded `./tools/test-pc-vpcd-applet` reached ATR and SELECT, then failed as expected because JCSystem info still returned `6F00`; teardown completed and no emulator process remained. No ARM build, RISC-V build, or hardware flash was run because this step changes only the PC test harness and documentation.
- Result: A failed APDU no longer leaves the long-running PC JCRE process behind. Class metadata for package 63/class 148 reports 13 declared instance words, `first_reference_token=0`, and `reference_count=9`; field token 11 resolves to the failing byte read while persisted slot 11 currently has short type. No field mapping fix is claimed.
- Decision: Inspect the v1.8.2 descriptor/class metadata and installation field layout to derive the token-to-slot mapping, then re-run JCSystem info.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `/tmp/pico2-jcalgtest-vpcd-cleanup.log`; prior full trace `artifacts/logs/host-vpcd-jcalgtest-20261014.log`.

## 2026-10-14 — Record bounded VPCD cleanup commit

- Step/experiment: `vpcd-test-process-group-cleanup-20261014-reference`
- Change: Recorded the immutable commit for the finite VPCD process-group cleanup and its tracker updates.
- Validation: `git diff --check -- progress.md` passed; commit `da33f39` contains the test harness, wrapper, TODO, handoff, and implementation record.
- Result: Bounded emulator teardown is committed as `da33f39`.
- Decision: Continue with field-token mapping and the still-failing JCSystem info APDU.
- Commit: recorded in the current commit.
- Artifacts/logs: `/tmp/pico2-jcalgtest-vpcd-cleanup.log`.

## 2026-10-15 — Refresh VPCD performance suggestions

- Step/experiment: `vpcd-performance-suggestions-refresh-20261015`
- Change: Updated `perf_tips.md` for the current VPCD trace, whose inherited
  `getReason()` lookup succeeds before an uncaught `SecurityException` at
  `GETFIELD_S_THIS` returns `6F00`. Added a profiling suggestion to aggregate
  filesystem-read and exception-handler timing before considering caching or
  indexing. Updated `.agent/HANDOFF.md` with the next diagnostic action.
- Validation: `./tools/task-time report` completed and matched the documented
  sample (52.821 seconds successful total). Inspected
  `/tmp/pico2-jcalgtest-getreason-fieldtrace.log` (215,263 bytes); no build,
  test, or hardware operation was run for this documentation-only step.
- Result: The note distinguishes observed trace volume (repeated `FsRead1b`
  diagnostics and a 51-entry exception-handler scan) from measured runtime
  cost; no speedup is claimed.
- Decision: Fix the current field-access failure, then collect bounded phase
  timing and aggregated counters on the same one-APDU reproducer.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `/tmp/pico2-jcalgtest-getreason-fieldtrace.log`.

## 2026-10-15 — Record performance-guidance commit

- Step/experiment: `vpcd-performance-suggestions-reference-20261015`
- Change: Recorded the immutable commit for the performance guidance refresh.
- Validation: `git diff --check -- progress.md` passed; commit `203d233`
  contains `perf_tips.md`, this step's progress record, and `.agent/HANDOFF.md`.
- Result: Current VPCD bottleneck suggestions and measurement caveats are
  committed as `203d233`.
- Decision: Address the `GETFIELD_S_THIS` correctness failure before collecting
  runtime performance samples.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `/tmp/pico2-jcalgtest-getreason-fieldtrace.log`.

## 2026-10-14 — Clarify VPCD performance measurement guidance

- Step/experiment: `vpcd-performance-timing-guidance-20261014`
- Change: Added the later roughly 20-second cleanup-run observation to `perf_tips.md`, explicitly marked it as informal and incomparable with the earlier smoke timing, and specified wall/user/system plus phase capture for the next run.
- Validation: `git diff --check -- perf_tips.md` passed. Documentation-only; no build, test, or hardware operation was run.
- Result: The note distinguishes an observed duration from a measured performance result and points to the next useful timing data.
- Decision: Capture comparable command and per-phase timings before recommending further VPCD optimization.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `/tmp/pico2-jcalgtest-vpcd-cleanup.log`.

## 2026-10-14 — Record VPCD performance-guidance commit

- Step/experiment: `vpcd-performance-timing-guidance-20261014-reference`
- Change: Recorded the commit reference for the VPCD timing guidance.
- Validation: `git diff --check -- progress.md` passed; commit `d249261` contains the suggestions and implementation record.
- Result: The performance note is committed as `d249261`.
- Decision: Use bounded phase and CPU-time measurements on the next VPCD run.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `/tmp/pico2-jcalgtest-vpcd-cleanup.log`.

## 2026-10-14 — Record handler-offset diagnostic timing

- Step/experiment: `jcalgtest-handler-offset-timing-20261014`
- Change: Added the latest bounded VPCD run's wall time, CPU time, exit result,
  and limitation to `perf_tips.md`; clarified that this failed run is not
  comparable as a speedup against earlier smoke runs.
- Validation: `git diff --check -- perf_tips.md progress.md` passed. Documentation-only; no build, test, or hardware operation was run.
- Result: The run took 7.39 s wall, 0.27 s user, 1.98 s system, 30% CPU, and 17,264 KiB peak RSS, then failed with `6F00`.
- Decision: Capture exact command and per-phase timings on the next VPCD run before attributing the elapsed time or tuning a phase.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `/tmp/pico2-jcalgtest-handler-base-fix.log`; `perf_tips.md`.

## 2026-10-14 — Record performance guidance commit

- Step/experiment: `jcalgtest-handler-offset-timing-reference-20261014`
- Change: Recorded the commit reference for the latest VPCD diagnostic timing note.
- Validation: `git diff --check -- progress.md` passed; commit `6444d59` contains the timing note, handoff update, and implementation record.
- Result: The measured failure and follow-up guidance are committed as `6444d59`.
- Decision: Collect exact wrapper and VPCD phase timings on the next run.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `/tmp/pico2-jcalgtest-handler-base-fix.log`.

## 2026-10-14 — Rank performance experiments from recorded timings

- Step/experiment: `rank-performance-experiments-20261014`
- Change: Added a ranked experiment table to `perf_tips.md` covering Maven stage timing/offline-mode evaluation, VPCD phase and child-process timing, comparable ARM build classes, focused versus full host tests, and separate hardware phase timing. Updated the VPCD note to reflect the current exception/method lookup failure rather than the superseded CP field 174 failure.
- Validation: `git diff --check -- perf_tips.md progress.md .agent/HANDOFF.md` passed. Documentation-only; no build, test, or hardware operation was run.
- Result: Recommendations now include the observed Maven 5:13 stage, VPCD 7.39 s failure timing, ARM build range 3.120–9.152 s, and explicit repeatable comparison criteria. These are single or variable samples, not claimed speedups.
- Decision: Collect per-stage VPCD timing and comparable build samples before tuning.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`; `/tmp/pico2-jcalgtest-handler-base-fix.log`.

## 2026-10-14 — Record performance-guidance commit

- Step/experiment: `rank-performance-experiments-reference-20261014`
- Change: Recorded the immutable commit for the ranked performance suggestions.
- Validation: `git diff --check -- progress.md` passed; commit `5b9ed64` contains the documentation update, handoff, and implementation record.
- Result: The new guidance is committed as `5b9ed64`.
- Decision: Add VPCD phase timings and collect comparable build samples before making performance changes.
- Commit: recorded in the current commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.
## 2026-10-15 — Summarize development performance suggestions

- Step/experiment: `development-performance-suggestions-20261015`
- Change: Added a concise category table and follow-up recommendations to
  `perf_tips.md`, based on the existing task timer sample. Distinguished failed
  or interrupted attempts from successful timing averages and marked upload as
  unmeasured. Updated the handoff with the current conclusions.
- Validation: `git diff --check -- perf_tips.md progress.md .agent/HANDOFF.md`
  passed. Documentation-only; no build, test, or hardware operation was run.
- Result: The sample records 17 successful intervals totaling 52.821 seconds
  (compile 25.633 s, tests 22.437 s, thinking 4.641 s, tool checks 0.110 s)
  and 8 non-OK intervals. The sample is observational, not a controlled
  benchmark.
- Decision: Use focused tests before firmware builds; collect like-for-like
  build samples and separate upload-phase timings before pursuing tuning.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Add current build and VPCD performance samples

- Step/experiment: `development-performance-followup-20261015`
- Change: Added this session's comparable build classes and bounded failed
  VPCD timing to `perf_tips.md`, and refreshed the active applet failure in
  `TODO.md` and `.agent/HANDOFF.md`.
- Validation: `./tools/build-pc` passed in 5.120 s; the PC `choupi` target
  passed in 30.790 s for a full build and three incremental rebuilds passed in
  9.916–10.778 s. `./tools/build arm` passed in 7.952 s and
  `./tools/build riscv` passed in 35.131 s. Three bounded VPCD attempts reached
  ATR and SELECT but failed the JCSystem info APDU with `6F00` in 5.341–5.974
  s. `git diff --check -- perf_tips.md TODO.md .agent/HANDOFF.md progress.md
  artifacts/metrics/development-time.csv` passed. No board flash.
- Result: The latest failure is `StackUnderflowException` at `PUTFIELD_A`
  (package 63, Method component offset 39, CP field index 5, depth 0); this is
  a correctness diagnostic, not an APDU performance result. Upload remains
  unmeasured.
- Decision: Continue tracing the static factory/native return path, validate
  CBC against known-answer vectors, and repeat the bounded VPCD test before
  flashing.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/metrics/development-time.csv`;
  `artifacts/logs/jcalgtest-putfield-callsite-20261015.log`;
  `perf_tips.md`.

## 2026-10-15 — Record performance follow-up commit

- Step/experiment: `development-performance-followup-reference-20261015`
- Change: Recorded commit `7fc1775` for the performance guidance, timing
  sample, TODO update, and handoff checkpoint.
- Validation: `git diff --check -- progress.md .agent/HANDOFF.md` passed.
- Result: The performance follow-up is committed as `7fc1775`.
- Decision: Continue with the applet operand-stack failure documented above.
- Commit: this reference commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Refresh measured performance suggestions

- Step/experiment: `refresh-performance-guidance-20261015`
- Change: Refreshed `perf_tips.md` with the current task-time totals, latest bounded VPCD failure, and distinctions between successful timings and failure-to-diagnosis durations. Updated recommendations to reflect current measurements while retaining unmeasured upload and hardware phases as open items.
- Validation: `./tools/task-time report` returned 26 successful intervals totaling 183.503 s and 17 non-OK intervals; `git diff --check -- perf_tips.md` passed. Documentation-only; no build, test, or hardware operation was run.
- Result: Compile intervals total 156.315 s (14 successful, mean 11.165 s, max 35.131 s); test intervals total 22.437 s (9 successful, 13 non-OK). Recent VPCD attempts took 5.341–5.974 s and failed at `PUTFIELD_A`, so they are not performance results.
- Decision: Use the refreshed suggestions to guide controlled, like-for-like measurements; avoid optimization claims from unlike build classes or failed APDU runs.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`; `artifacts/logs/jcalgtest-putfield-callsite-20261015.log`.

## 2026-10-15 — Record performance guidance commit

- Step/experiment: `refresh-performance-guidance-reference-20261015`
- Change: Recorded commit `8e5efa7` for the updated performance suggestions and current timer summary.
- Validation: `git diff --check -- progress.md` passed.
- Result: The refreshed performance guidance is committed as `8e5efa7`.
- Decision: Continue using comparable, phase-specific measurements before proposing further speedups.
- Commit: recorded in this reference commit.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Add current performance suggestions

- Step/experiment: `task-time-performance-suggestions-20261015`
- Change: Added current category totals and suggestions to `perf_tips.md`,
  including separate upload-phase timing, resource measurements for long
  commands, bounded device retries, and serial build experiments for the
  eight-core VM. Clarified that recent VPCD timing is a failed diagnostic, not
  successful applet performance. Updated `.agent/HANDOFF.md` with the same
  measurement checkpoint.
- Validation: `./tools/task-time report` showed 26 successful intervals,
  183.503 s total, and 18 non-OK intervals; `git diff --check` passed.
  Documentation-only; no build, test, or hardware operation was run.
- Result: Upload remains unmeasured. Latest VPCD failure timing is 13.116 s and
  ends in `6F00` at the `CHECKCAST` / `SecurityException` path.
- Decision: Use comparable, phase-specific measurements after the runtime
  failure is fixed; do not infer CPU bottlenecks or speedups from this sample.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`;
  `artifacts/logs/jcalgtest-class-resolution-fix-20261015.log`.

## 2026-10-15 — Record performance suggestions commit

- Step/experiment: `task-time-performance-suggestions-reference-20261015`
- Change: Recorded commit `536445f` for the current performance suggestions,
  task-time summary, and handoff update.
- Validation: `git diff --check -- progress.md` passed.
- Result: The documentation step is committed; no code or hardware behavior
  changed.
- Decision: Continue with the JCAlgTest `CHECKCAST` failure; gather new
  performance samples only from comparable work units.
- Commit: recorded in this reference commit.
- Artifacts/logs: `perf_tips.md`; `.agent/HANDOFF.md`.

## 2026-10-15 — Refresh development performance suggestions

- Step/experiment: `performance-suggestions-refresh-20261015`
- Change: Updated `perf_tips.md` with the current timer summary table, current
  VPCD `INVOKEVIRTUAL` stack-underflow location, and measurement priorities.
  Marked prior timing and failure diagnoses as historical. Updated `TODO.md`
  and `.agent/HANDOFF.md` to match the current failing opcode and trace.
- Validation: `./tools/task-time report` showed 34 successful intervals
  totaling 286.750 s plus 28 non-OK intervals; `git diff --check` passed.
  Documentation-only; no build, test, or hardware operation was run.
- Result: Compile accounts for 259.562 s across 22 successful intervals;
  upload remains unmeasured. Latest VPCD failure took 15.897 s and returned
  `6F00` at `INVOKEVIRTUAL` after ATR and SELECT.
- Decision: Use phase timing and CPU/resource samples before proposing runtime
  or build changes; continue the current operand-stack diagnosis.
- Commit: `6057c7d`.
- Artifacts/logs: `perf_tips.md`; `artifacts/metrics/development-time.csv`;
  `artifacts/logs/jcalgtest-interface-hierarchy-fix-20261015.log`.

## 2026-10-15 — Record performance guidance commit

- Step/experiment: `performance-suggestions-refresh-reference-20261015`
- Change: Recorded commit `6057c7d` for the current performance suggestions,
  task-time summary, and continuity updates.
- Validation: `git diff --check -- progress.md` passed.
- Result: The performance guidance update is committed; no build, test, or
  hardware behavior changed.
- Decision: Continue with the JCAlgTest operand-stack diagnosis and collect
  comparable phase/resource measurements when they can guide an optimization.
- Commit: recorded in this reference commit.
- Artifacts/logs: `perf_tips.md`; `.agent/HANDOFF.md`; `TODO.md`.

## 2026-10-15 — Fix ROM-mask internal virtual method tokens

- Step/experiment: `jca-rommask-internal-virtual-method-table-tokens-20261015`
- Change: ROM-mask generation now resolves an internal virtual-method signature
  to its class public/package method-table token instead of using the method
  declaration index. This corrected APDU virtual calls that previously encoded
  token zero. Nested ROM-mask source commit: `fa470e5`. Updated the current
  JCAlgTest checkpoint in `TODO.md` and `.agent/HANDOFF.md`.
- Validation: `./tools/build-pc` passed; generated framework APDU CP tokens are
  `1, 3, 5, 8` for `getBuffer`, `setOutgoing`, `setOutgoingLength`, and
  `sendBytes`. The PC emulator build passed. A bounded one-pass
  `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 20` passed ATR,
  SELECT, JCSystem info, and supported-algorithm APDUs (`TEST:SUMMARY pass=5
  fail=0`). `./tools/build arm` passed after marking the PC-only diagnostic
  stage variable `[[maybe_unused]]` in `bc_object.cpp`; the ARM build initially
  failed because `-Werror` treated it as unused. No RISC-V build/test or flash
  was run.
- Result: The former `INVOKEVIRTUAL` underflow is cleared on the PC VPCD path.
  This proves one host-emulator pass only; physical applet execution, the full
  official host client, repeated clean runs, and power-cycle persistence remain
  unverified. The `bc_object.cpp` warning annotation is in a file with broader
  pre-existing uncommitted changes and was not included in this step's commit.
- Decision: Remove or gate temporary diagnostics, verify repeatability, run the
  official supported-algorithms client against VPCD, and add CBC KAT coverage.
  Continue ARM-only and defer flashing until repository build gates permit it.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`;
  `build/choupi-pc/flash`; `build/arm/pico2_choupi.uf2`.

## 2026-10-15 — Record virtual-token fix commit

- Step/experiment: `jca-rommask-internal-virtual-method-table-tokens-reference-20261015`
- Change: Recorded root commit `1104520` for the JCAlgTest VPCD milestone,
  TODO/handoff refresh, and progress entry. The ROM-mask implementation itself
  is committed in its nested repository as `fa470e5`.
- Validation: `git diff --check` passed before commit; root commit contains only
  `TODO.md`, `.agent/HANDOFF.md`, and `progress.md`.
- Result: Current one-pass VPCD and ARM build evidence is linked from the
  handoff. This does not establish physical-board acceptance criteria.
- Decision: Continue temporary-trace cleanup and repeated VPCD/client checks.
- Commit: recorded in this reference commit.
- Artifacts/logs: `artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`.

## 2026-10-15 — Refresh measured performance suggestions

- Step/experiment: `performance-guidance-refresh-20261015`
- Change: Updated `perf_tips.md` with the current task-time report totals and
  the passing JCAlgTest VPCD checkpoint. Refined recommendations to measure
  APDU phases, CPU/RSS, bounded retry cost, comparable build classes, and
  upload/re-enumeration separately.
- Validation: `./tools/task-time report` produced 38 successful intervals
  totaling 439.623 s and 37 non-OK intervals; `git diff --check -- perf_tips.md`
  passed. No build, test, or hardware operation was needed for this
  documentation-only step.
- Result: The current documentation no longer presents the pre-fix VPCD failure
  as the latest result. It clearly marks 18.215 s as end-to-end host smoke time,
  not card throughput; upload remains unmeasured.
- Decision: Use per-phase and resource measurements for the next performance
  decisions; do not tune concurrency or VM lookup from these mixed samples.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `perf_tips.md`,
  `artifacts/logs/jcalgtest-internal-virtual-table-token-fix-20261015.log`,
  `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Gate release host syscall diagnostics

- Step/experiment: `host-release-syscall-log-gate-20261015`
- Change: Compile the host OS syscall detail prints only with Rust debug
  assertions enabled, preventing per-syscall `println!` output in the release
  emulator. Refreshed `perf_tips.md` and `.agent/HANDOFF.md` with the current
  timing and repeatability results. The previous performance-doc commit is
  `97139d2`.
- Validation: `./tools/build-pc` passed; the PC `choupi` target built with the
  existing packed-member warnings; `./tools/build arm` passed. The bounded
  release VPCD test passed twice: `TEST:SUMMARY pass=10 fail=0` in 47.575 s.
  A one-pass `/usr/bin/time` sample reported 23.48 s wall, 0.48 s user, 6.61 s
  system, 30% CPU, and 17,128 KiB max RSS. `git diff --check` passed for the
  documentation files. No RISC-V build or physical flash was run.
- Result: The release emulator log is 47,015 bytes for one pass, down from
  291,641 bytes in the preceding timed-out release attempt. The run passed,
  but these single samples do not establish a runtime speedup or pinpoint the
  wall-time cost.
- Decision: Keep release diagnostics quiet. Next run the official JCAlgTest
  supported-algorithms client against VPCD and compare its CSV with the
  simulator reference; continue measuring transport and emulator phases before
  changing execution or wait logic.
- Commit: nested OS source commit and root documentation commit are recorded in
  the following reference entry.
- Artifacts/logs: `artifacts/logs/jcalgtest-vpcd-release-after-log-gate-20261015.log`,
  `artifacts/logs/jcalgtest-vpcd-release-repeat-20261015.log`,
  `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Record release logging commits

- Step/experiment: `host-release-syscall-log-gate-reference-20261015`
- Change: Record the nested CHOUPI OS commit `ac1df19` and root documentation
  commit `3044579`; also record the preceding performance guidance commit
  `97139d2`.
- Validation: Confirmed the nested commit contains only the two release
  diagnostic gates and the root commit contains only `perf_tips.md`,
  `.agent/HANDOFF.md`, and this step's progress record.
- Result: The quiet-release change and its measurement notes are independently
  committed without staging other existing working-tree changes.
- Decision: Continue with the official JCAlgTest supported-algorithms client
  against the PC VPCD path.
- Commit: recorded in this reference entry.
- Artifacts/logs: `artifacts/logs/jcalgtest-vpcd-release-repeat-20261015.log`.

## 2026-10-15 — Add VPCD phase timings and refresh measurements

- Step/experiment: `vpcd-phase-timing-20261015`
- Change: The bounded VPCD harness now reports monotonic durations for connect,
  ATR, SELECT, each tested APDU, emulator shutdown, and total pass time. Updated
  `perf_tips.md`, `TODO.md`, and `.agent/HANDOFF.md` with the phase results and
  the current PC/SC/VPCD prerequisite blocker.
- Validation: `python3 -m py_compile tools/tests/pc_vpcd_applet_test.py` passed;
  one bounded release VPCD pass passed with `TEST:SUMMARY pass=5 fail=0` in
  23.835 s. Measured phases: connect 2.554 s, ATR 0.001 s, SELECT 0.231 s,
  JCSystem info 16.437 s, supported-algorithm APDU 4.561 s, emulator stop
  0.031 s. `./tools/task-time report` now shows 46 successful intervals
  totaling 589.081 s and 41 non-OK intervals. No physical card was used.
- Result: The JCSystem info APDU dominates this one host timing sample. More
  repetitions and per-phase CPU counters are needed before assigning a root
  cause. The official client cannot run in this environment because PC/SC
  reports service unavailable and `vsmartcard-vpcd` is not installed.
- Decision: Keep phase output bounded and machine-readable. Once the host VPCD
  PC/SC driver and service are available, run JCAlgTest v1.8.2 supported
  algorithms mode and compare the output with the jCardSim reference.
- Commit: recorded in the following reference entry.
- Artifacts/logs: `artifacts/logs/jcalgtest-vpcd-phase-timing-20261015.log`,
  `artifacts/metrics/development-time.csv`.

## 2026-10-15 — Answer VPCD ATR presence probes before power-on

- Step/experiment: `vpcd-atr-presence-probe-20261015`
- Change: VPCD now returns its ATR when the reader probes card presence even
  before power-on. The previous powered-state guard made PC/SC report
  `NO CARD`; JCAlgTest then received a null output stream and crashed while
  writing the CSV header.
- Validation: `./tools/test-vpcd-session` passed (`TEST:SUMMARY pass=14
  fail=0`). `./tools/build-pc` and the PC `choupi` target build passed. Cargo
  emitted existing warnings in the host OS dependency; the changed C++ target
  compiled and linked without warnings. No live PC/SC retry or hardware test
  was run after the fix.
- Result: The pre-power ATR probe is covered by the focused session test. The
  official client still needs a rerun with the rebuilt emulator to confirm
  PC/SC sees the virtual card and proceeds to APDUs.
- Decision: Keep the ATR independent of powered state for presence checks;
  validate with official JCAlgTest once the user restarts the emulator.
- Commit: recorded in the following reference entry.
- Artifacts/logs: no new log; user-provided JCAlgTest output shows the
  `NO CARD` reader state and null-stream exception.

## 2026-10-15 — Record VPCD ATR probe fix commit

- Step/experiment: `vpcd-atr-presence-probe-reference-20261015`
- Change: Recorded implementation commit `ceb5101`, containing the unpowered
  ATR response, focused session expectation, handoff update, and validation
  record.
- Validation: Confirmed commit `ceb5101` contains only
  `choupi/src/vpcd_session.cpp`, `tools/tests/vpcd_session_test.cpp`,
  `.agent/HANDOFF.md`, and its progress record.
- Result: The implementation fix and its validation are isolated from unrelated
  pre-existing worktree changes.
- Decision: Wait for the official client rerun against the rebuilt emulator to
  confirm the PC/SC reader now reports a present card.
- Commit: recorded in this reference entry.
- Artifacts/logs: none.

## 2026-10-15 — Record VPCD phase timing commit

- Step/experiment: `vpcd-phase-timing-reference-20261015`
- Change: Recorded root commit `6d633f3`, which contains the phase-timing
  harness, refreshed performance suggestions, TODO checkpoint, and handoff.
- Validation: Confirmed the implementation commit contains only
  `tools/tests/pc_vpcd_applet_test.py`, `perf_tips.md`, `TODO.md`,
  `.agent/HANDOFF.md`, and its progress record.
- Result: Phase timing and the current official-client prerequisite blocker are
  recorded for the next session.
- Decision: Continue after enabling the virtual PC/SC reader; the repository
  build and test work that does not depend on that host service remains
  available.
- Commit: recorded in this reference entry.
- Artifacts/logs: `artifacts/logs/jcalgtest-vpcd-phase-timing-20261015.log`.

## 2026-10-15 — Revalidate VPCD emulator after PC/SC connection refusal

- Step/experiment: `vpcd-internal-revalidation-20261015`
- Change: Re-ran the repository's bounded host VPCD applet harness against the
  current rebuilt emulator and checked reachability of the user's PC/SC VPCD
  port from the agent execution environment.
- Validation: `./tools/test-pc-vpcd-applet --passes 1 --log
  artifacts/logs/jcalgtest-vpcd-internal-after-atr-fix-20261015.log
  --flash-copy build/algtest-v182-vpcd-flash-after-atr-fix.bin` passed with
  `TEST:SUMMARY pass=5 fail=0`. Phases: connect 1.789 s, ATR 0.001 s, SELECT
  0.161 s, JCSystem info 13.934 s, supported-algorithm APDU 2.945 s. A direct
  TCP probe from the agent namespace returned `Connection refused`, even with
  escalation; the user separately observed LISTEN on ports 35963/35964.
- Result: The current binary is fresh and its internal VPCD/APDU path passes.
  The official PC/SC integration remains unverified because the listener shown
  in the user's VM is not reachable from this command execution namespace.
- Decision: For the official client check, run the emulator in the same VM
  network namespace as the active `pcscd` VPCD listener; compare `ss -ltnp`
  from that namespace at the time of connection.
- Commit: recorded in the following reference entry.
- Artifacts/logs:
  `artifacts/logs/jcalgtest-vpcd-internal-after-atr-fix-20261015.log`.

## 2026-10-15 — Isolate JCAlgTest HMAC-SHA1 timeout

- Step/experiment: `vpcd-signature-sequence-hmac-sha1-20261015`
- Change: Updated `.agent/HANDOFF.md` with the official PC/SC timeout evidence and corrected the earlier assumption that Codex was in a separate VM/network namespace. No source code changed.
- Validation: Replayed signature algorithms 1–24 against the rebuilt PC emulator through the bounded internal VPCD harness, sending `B0 E2 00 00 00` before each `B0 75 12 00 01 <algorithm> 00` APDU. The full run passed with `TEST:SUMMARY pass=5 fail=0`. A 12-second watchdog attempt ended during the normal JCSystem info preamble, so the successful rerun used a 25-second APDU deadline and 180-second overall cap. Separately confirmed a 20-second-bounded local socket attempt is denied by the default sandbox (`EPERM`); the replay succeeded with host socket permission. The official Java PC/SC run had previously reached its 300-second timeout at algorithm 24.
- Result: All 24 ordered signature APDUs returned `9000`; HMAC-SHA1 took 3.026 s, and the full replay took 97.284 s. This does not reproduce the PC/SC-only stall. No hardware was used.
- Decision: Restart the PC/SC VPCD service and capture an official-client rerun with per-APDU deadlines and emulator logs before changing VM runtime code.
- Commit: `37b0a90`.
- Artifacts/logs: `/tmp/algtest-v182-signature-sequence.log` (SHA-256 `d3fecd2f656c27a2cd3d6b38b176dd526ef81ca47774e0cd94a0447df27f5f64`); official partial output `artifacts/results/jcalgtest-vpcd/ALGTEST_log_1792050525367.log`.

## 2026-10-15 — PC/SC service restart permission boundary

- Step/experiment: `pcscd-restart-from-local-codex-20261015`
- Change: Recorded the system-service control boundary in `.agent/HANDOFF.md`; no service configuration or source code changed.
- Validation: Attempted `systemctl start pcscd.service` without sudo using the host-access command path. It timed out connecting to the system bus. A subsequent process/socket check showed no pcscd process and no VPCD TCP listeners; the existing `/run/pcscd/pcscd.comm` is a stale socket. No test or hardware operation was run.
- Result: The local VPCD harness works with host-socket permission, but this execution environment cannot control the system pcscd service through system D-Bus. No test processes remain running.
- Decision: User must run `sudo systemctl restart pcscd.service` and confirm `pcsc_scan -r` lists the virtual readers before the official Java client can be replayed here.
- Commit: `09977e5`.
- Artifacts/logs: systemctl reported `Failed to start pcscd.service: Connection timed out`; follow-up system bus query reported `Operation not permitted`.

## 2026-10-15 — Run JCAlgTest through the virtual PC/SC reader

- Step/experiment: `official-jcalgtest-vpcd-pcsc-20261015`
- Change: Verified the user's restarted PC/SC service from the local session and
  ran the official JCAlgTest v1.8.2 client against the CHOUPI PC emulator. No
  source code changed.
- Validation: `timeout 5s pcsc_scan -r` listed two virtual readers. With the
  emulator connected to `127.0.0.1:35963`, `pcsc_scan` reported card inserted
  on `Virtual PCD 00 00`, ATR `3B 80 01 81`. The official Java client selected
  the applet successfully and sent algorithm APDUs through SunPCSC. It ran at
  low priority with a 90-second timeout and was intentionally stopped at the
  cap; no test/build/flash of the physical Pico was performed.
- Result: CSV contains 14 algorithm rows. Observed probes took 3.8–5.7 s each;
  the first AES CBC NOPAD probe was reported supported. The client also
  receives `6F00` for GETVERSION and JCSystem info, as seen in the prior full
  run. Early client attempts returned `NO CARD` until the emulator's inserted
  card state appeared in PC/SC.
- Decision: Use the PC emulator for continued JCAlgTest debugging; the
  remaining blocker is the applet's metadata `NullPointerException`, not
  physical-token access. Investigate the version-field initialization before
  attempting a longer suite run.
- Commit: `e75f5ad`.
- Artifacts/logs:
  `artifacts/results/jcalgtest-vpcd-pcsc-probe-20261015/ALGTEST_log_1792052906094.log`
  (SHA-256 `f7854659886cbbd341f6e03bfe4798bccce947ab07202572a90aa9d8709e6ee2`)
  and `CHOUPI_VPCD_ALGSUPPORT__3b_80_01_81.csv` (SHA-256
  `b38c55958995a6c5328b262ca526f6fc92cb4149c15b874a7e0a07f2df7da069`).
## 2026-10-15 — JCAlgTest install and JCSystem VPCD smoke test

- Step/experiment: `jcalgtest-vpcd-jcsystem-array-size-20261015`
- Change: Initialize CAP `StaticField` array initializers during transactional installation; correct serialized primitive-array offsets and interpret persistent array header lengths as element counts; make the VPCD test install JCAlgTest 1.8.2 and assert version, JCSystem, and supported-algorithm responses.
- Validation: `./tools/env cmake --build build/choupi-pc --target choupi -j2` passed; `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 20 --log artifacts/logs/jcalgtest-jcsystem-size-fix-20261015.log` passed with `TEST:SUMMARY pass=5 fail=0`; `./tools/build arm` passed. No RISC-V build or physical flash was performed.
- Result: ATR `3B800181`; GETVERSION returned exact `1.8.2_jc305`; JCSystem info returned `0305000000000000000000002000FE01000000000000000000000000009000`; supported-algorithm response ended in `9000`. VPCD pass took 27.851 s. The JCSystem failure was caused by dividing a serialized short-array element count by two.
- Decision: Continue JCAlgTest coverage through the local emulator. Physical-token validation remains a separate hardware step; this change was validated on VPCD only.
- Commit: `33af96f`.
- Artifacts/logs: `artifacts/logs/jcalgtest-jcsystem-size-fix-20261015.log`; ARM image `build/arm/pico2_choupi.uf2`.

## 2026-10-15 — Repeat JCAlgTest emulator smoke test

- Step/experiment: `jcalgtest-vpcd-jcsystem-repeat-20261015`
- Change: Repeated the fresh-install smoke path twice to check install-time static arrays and JCSystem handling across clean emulator runs; no source code changed.
- Validation: `./tools/test-pc-vpcd-applet --passes 2 --apdu-timeout 20 --log artifacts/logs/jcalgtest-jcsystem-repeat-20261015.log` passed with `TEST:SUMMARY pass=10 fail=0`.
- Result: Both clean passes returned ATR `3B800181`, exact version `1.8.2_jc305`, JCSystem info ending `9000`, and the supported-algorithm response ending `9000`. Pass times were 27.789 s and 29.546 s. PC/SC was unavailable during this check; direct VPCD was used. No token was used.
- Decision: Treat the VPCD smoke result as repeatable and continue with bounded cipher algorithm coverage. Hardware validation is still not established.
- Commit: `33af96f` (implementation under repeated validation).
- Artifacts/logs: `artifacts/logs/jcalgtest-jcsystem-repeat-20261015.log` (SHA-256 `def236f08168911e5873b54084c11a108307dd9e55e6252697156e2d4afb0866`).

## 2026-10-15 — Probe AES-CBC-NOPAD through JCAlgTest

- Step/experiment: `jcalgtest-vpcd-aes-cbc-nopad-20261015`
- Change: Extended the VPCD smoke harness to probe `Cipher.ALG_AES_BLOCK_128_CBC_NOPAD` and require JCAlgTest to report it supported; updated the summary counter to include the additional assertion.
- Validation: `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 20 --log artifacts/logs/jcalgtest-aes-cbc-support-verified-20261015.log` passed with `TEST:SUMMARY pass=6 fail=0`.
- Result: AES-CBC-NOPAD returned data prefix `1100` and status `9000`, confirming the client-visible supported result. Total emulator pass time was 30.396 s. No PC/SC or physical token was used.
- Decision: Retain this as the first direct cryptographic capability probe; next add a bounded sweep across JCAlgTest algorithm families while keeping each APDU time-limited.
- Commit: `16fe4b7`.
- Artifacts/logs: `artifacts/logs/jcalgtest-aes-cbc-support-verified-20261015.log` (SHA-256 `4061b2ef91a2e35f19211a77d1c7d3c7f8641a35391926c54da06cc97d70d438`).

## 2026-10-15 — Probe JCAlgTest algorithm families over VPCD

- Step/experiment: `jcalgtest-vpcd-family-probes-20261015`
- Change: Added bounded single-mode probes for RSA-SHA1 Signature, pseudo-random RandomData, and CRC16 Checksum alongside the existing AES-CBC-NOPAD probe; each probe requires a complete response and `9000` status.
- Validation: `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 20 --log artifacts/logs/jcalgtest-family-probes-20261015.log` passed with `TEST:SUMMARY pass=9 fail=0`.
- Result: Cipher AES-CBC-NOPAD reported supported (`1100`); RSA-SHA1 Signature, pseudo-random RandomData, and CRC16 Checksum each reported `03` (`NO_SUCH_ALGORITHM`) while returning status `9000`. Full emulator pass took 42.627 s. No PC/SC or physical token was used.
- Decision: Keep the broader harness as a smoke test and treat the three `03` results as known unsupported algorithms, not transport/runtime faults. Next choose one unsupported family and implement the smallest missing Java Card API path.
- Commit: `3bf05dd`.
- Artifacts/logs: `artifacts/logs/jcalgtest-family-probes-20261015.log` (SHA-256 `245aa44a2cec1482a8ddd2b740a6f26c100fd76c55b5eb190ad9438bbb675c9b`).

## 2026-10-15 — Rename leftover smoke target to JCVM

- Step/experiment: `rename-pico2-trng-target-20261015`
- Change: Renamed the root CMake project from `pico2_trng` to `pico2_jcvm` and the bare-board smoke executable/artifact to `pico2_jcvm_smoke`; updated the build and cycle wrappers and the compilation reference. Kept actual TRNG and random-number API terminology intact.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed, producing `build/arm/pico2_jcvm_smoke.uf2` and `build/riscv/pico2_jcvm_smoke.uf2`; picotool recognized the RP2350 ARM Secure and RISC-V images. No smoke image was flashed after the rename.
- Result: Build outputs now use project names instead of the Pico SDK TRNG template name. The separate CHOUPI hardware cycle performed during this turn is recorded below.
- Decision: Use the renamed smoke artifacts for subsequent build checks; continue validating JCAlgTest applet behavior separately.
- Commit: `ccb7605`.
- Artifacts/logs: ARM UF2 SHA-256 `ac8c3c596188862120b6484c0ef60b3dfc632f2c3d7b392efdffa45d22d4007b`; RISC-V UF2 SHA-256 `dbd9ff748a05b72c6554a1af964215c2720f0c6cf738b9282d07bbe610415b87`.

## 2026-10-15 — Flash CHOUPI to real Pico 2

- Step/experiment: `physical-choupi-jcsystem-smoke-20261015`
- Change: Flashed the current ARM CHOUPI UF2 to the attached Pico 2 using `./tools/cycle choupi`; no source change was made for this hardware experiment.
- Validation: ARM and RISC-V builds passed before flashing. Picotool reported 100% loaded and rebooted the board. The 18-second finite serial capture failed the cycle health check: repeated `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`, and `TEST:SUMMARY pass=0 fail=1`; CCID remained at `stage=2 endpoint=00 rx_events=0 tasks=0`. `pcsc_scan -r` showed only the two Virtual PCD readers, not the physical Pico reader. No applet APDUs could be sent to the board.
- Result: Physical firmware download and CDC serial capture work. Full applet/JCSystem behavior is not yet verified on hardware; the boot entry currently returns `6F00`, and the physical CCID reader is unavailable through PC/SC in the current session.
- Decision: Inspect why the generated startup entry returns `6F00`, then restore physical PC/SC access and install JCAlgTest 1.8.2 before judging applet or JCSystem behavior. Treat `NO_SUCH_ALGORITHM` responses as expected, per user scope.
- Commit: `ccb7605` (rename checkpoint; this hardware result is documentation only).
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261015T094100Z.log` (SHA-256 `cac6eedfa0a1147323ed0e224444d99cfc8234fcc6e1bf8b8be8207a526f1422`); flashed UF2 SHA-256 `d0dcb43abdae352027064def9495140cc1785557121f289537aed1c1d35a8e9e`.

## 2026-10-15 — Diagnose physical applet restore read

- Step/experiment: `physical-applet-restore-read-diagnostic-20261015`
- Change: Added a narrowly scoped serial trace at persistent-reference reads to report the exception, root tag, existence flag, and decoded record type. This distinguishes an absent instance root from a filesystem read failure without changing restore behavior.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` rebuilt both architectures but flashing failed before upload because picotool could not initialize libusb (`status=250`); therefore no hardware validation was performed. `sudo -n /usr/local/sbin/pico2-pcsc-reader` was also blocked by this execution context's `no_new_privileges` setting.
- Result: The prior hardware trace still establishes that SELECT matches package 63/class `0x0094` and restore returns null. The new diagnostic is compiled but not yet run on the board. Current USB access is unavailable to this session (`lsusb`: libusb `-99`).
- Decision: Preserve the new trace and rerun the physical SELECT probe when USB access is restored; do not claim applet/JCSystem hardware verification yet.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261015T101749Z.log` (SHA-256 `3f6e644789143bebc2394f7b756f452d417d52bc7d23132a9428f042297f5cb3`); ARM UF2 SHA-256 `653622ffd0cbb12e1e3435b9a00926572d5cffa8b87a72a4d6edb7da6c74eee9`.

## 2026-10-15 — Make restore diagnostics safe on denied reads

- Step/experiment: `physical-applet-restore-diagnostic-safety-20261015`
- Change: Removed the diagnostic's secondary `fs_exists()` call, which could repeat the filesystem access check after a failed read. The restore root now logs the original `fs_read_inplace` status and initializes its output length before the call.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. No test was added or run. Hardware validation was skipped because picotool cannot initialize libusb in this execution context.
- Result: The diagnostic reports the primary read status without issuing another filesystem syscall; runtime restore behavior is unchanged.
- Decision: Keep this safe trace and use it on the board once USB passthrough is accessible to the session.
- Commit: `c290cd1`.
- Artifacts/logs: ARM UF2 SHA-256 `5c8cab86d6eff1bed98decefaf0dc89005f611a15203857f293b95f144f2b8a8`; the prior blocked flash attempt is recorded in `artifacts/logs/cycle-choupi-20261015T101749Z.log`.

## 2026-10-15 — Compare applet restore through VPCD

- Step/experiment: `jcalgtest-vpcd-restore-compare-20261015`
- Change: Ran the existing bounded JCAlgTest 1.8.2 applet smoke test against the PC VPCD emulator; no source code changed.
- Validation: The sandbox first denied local socket use (`EPERM`); rerunning the same bounded test with approved host-socket access passed with `TEST:SUMMARY pass=9 fail=0`.
- Result: VPCD installation wrote root tag `03003F94FF` with a four-byte object header. SELECT returned `9000`; GETVERSION returned `1.8.2_jc305`; JCSystem info returned `0305000000000000000000002000FE01000000000000000000000000009000`. Unsupported RSA-SHA1 and CRC16 probes returned `NO_SUCH_ALGORITHM` with `9000`, as expected. Total emulator pass time was 58.436 s.
- Decision: The host emulator's install, persisted applet restore, APDU handling, and JCSystem info path pass. Physical restore remains unverified; compare the new root-read status on hardware once USB access is restored.
- Commit: `c290cd1` (diagnostic implementation under comparison; this step changes no source).
- Artifacts/logs: `artifacts/logs/jcalgtest-restore-after-safe-diagnostic-20261015.log` (SHA-256 `6f7c41916df69e2a4d443588f5ce914d18a3ac1acb01c5dd5a4e82a40506d629`).

## 2026-10-15 — Reflash physical Pico 2 for restore tracing

- Step/experiment: `physical-choupi-restore-diagnostic-flash-20261015`
- Change: Rebuilt both targets and flashed the ARM CHOUPI image to the Pico 2 in BOOTSEL; no source change was made in this hardware step.
- Validation: `./tools/cycle choupi` rebuilt ARM and RISC-V; picotool loaded the image to 100% and rebooted the board. The finite 18-second serial health check failed: repeated `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`; CCID debug remained at stage 2/endpoint 0 with no APDU traffic. USB enumerated `2e8a:0009`, but `pcsc_scan -r` listed only the two virtual readers. The bounded probe reported physical `Pico 2 CHOUPI CCID` not found by PC/SC; no APDU was sent.
- Result: The board accepted the diagnostic firmware. The physical root-read diagnostic has not run because PC/SC is unavailable to this execution context. Device node `/dev/bus/usb/001/023` grants ACL to `claude`, but not to service account `pcscd`; restarting the system service requires the already-installed privileged helper.
- Decision: Continue physical SELECT verification once PC/SC access is restored. The repository's `AGENTS.md` forbids invoking sudo from this autonomous cycle; ask the user to run `sudo -n /usr/local/sbin/pico2-pcsc-reader`, then confirm `pcsc_scan -r` lists the physical reader before probing.
- Commit: `c290cd1` (diagnostic implementation under physical validation; no source change in this step).
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261015T112920Z.log` (SHA-256 `369ae5865b5645067ab74e22811b18510c1cd290945ac1b510579bd8cbcb48de`); flashed ARM UF2 SHA-256 `5c8cab86d6eff1bed98decefaf0dc89005f611a15203857f293b95f144f2b8a8`.
## 2026-10-15 — Confirm physical restore failure is volatile filesystem state

- Step/experiment: `physical-jcalgtest-restore-after-pcsc-enable-20261015`
- Change: No firmware source changed. Used the enabled physical PC/SC reader to inspect the GlobalPlatform registry and issue a bounded JCAlgTest SELECT while capturing 45 seconds of serial output.
- Validation: GlobalPlatformPro `--list` connected to the Pico and listed ISD `A000000151000000` plus applet instance `4A43416C675465737431` (lifecycle `INITIALIZED`). `./tools/probe-jc-algtest` connected with ATR `3B800181`, then SELECT returned `6F00`; command exited 1. The serial trace matched the SELECT AID and package 63/class `0094`, then reported `RESTORE_STORAGE status=1 length=0`, `RESTORE_READ_FAIL id=5 tag=03003F94FF`, and `SELECT restore=1`.
- Result: The C ABI filesystem shim maps status 1 to a missing record. Its `fs_init()` clears the in-memory overlay at every startup; `cap_store` restores CAP bytes and registry metadata but not the applet instance root. This explains why the registered applet cannot be restored after firmware restart. Physical applet execution and JCSystem APDUs remain unverified on this boot.
- Decision: Fix or replace the RP2350 RAM-only overlay persistence path before claiming successful restart/SELECT behavior. Keep the reserved CAP slot intact and validate flash layout before writing any persistent records.
- Commit: pending
- Artifacts/logs: `artifacts/logs/jcalgtest-physical-restore-recheck-20261015.log` (SHA-256 `ba9e4a602c03667c8327109d94c56983d2ff3f67077362990926b1f210239376`); read-only GP listing returned the registered applet instance.
## 2026-10-15 — Verify JCAlgTest and JCSystem on the physical Pico 2

- Step/experiment: `physical-jcalgtest-jcsystem-apdu-20261015`
- Change: Extended the bounded PC/SC probe to verify that unsupported RSA-SHA1 Signature is reported as `NO_SUCH_ALGORITHM` (`12 03`) while the APDU still returns `9000`.
- Validation: Reinstalled the supplied `AlgTest_v1.8.2_jc305.cap` on the running physical card; GlobalPlatformPro completed LOAD and INSTALL with `9000`. The physical probe passed SELECT (`9000`), exact GETVERSION `1.8.2_jc305`, JCSystem info (`9000`), and RSA-SHA1 unsupported-result checks (`12 03`, `9000`). `./tools/build arm` and `./tools/build riscv` passed. A finite 15-second serial capture completed, but it continues to report `CHOUPI:STATUS sw=6F00` / `TEST:FAIL choupi_runtime` from the firmware's boot-time runtime check; this remains a failure even though later APDU checks passed. No firmware was flashed because only the host-side probe changed and reflashing would reset the currently installed RAM-only instance state.
- Result: Live physical SELECT, JCAlgTest 1.8.2 execution, and its JCSystem info path work after installation. The info APDU exercises `getVersion`, `isObjectDeletionSupported`, scalar and array forms of `getAvailableMemory`, and `getMaxCommitCapacity`. The applet correctly returns `NO_SUCH_ALGORITHM` for the unsupported RSA-SHA1 request. Power-cycle restore remains broken: `fs_init()` clears the RP2350 RAM overlay, so applet root and persistent fields disappear after restart. The boot-time `TEST:FAIL` also remains unresolved.
- Decision: Keep this as a live-session hardware pass only. Next implement persistent RP2350 filesystem storage and correct the boot health check, then test a reset and repeat the same physical APDUs.
- Commit: pending
- Artifacts/logs: install log `artifacts/logs/gp-install-physical-live-recovery-20261015.log` (SHA-256 `fca4e736a1e7dd4cc2d76e589e1e33504ba0b33798c2e9dabe52a365c4cc2020`); APDU log `artifacts/logs/jcalgtest-physical-jcsystem-unsupported-20261015-apdu.log` (SHA-256 `2991de425250d2a239fd202a3c03030648f3d1571f9ea3d16ca53c2d7f53c5a8`); serial log `artifacts/logs/jcalgtest-physical-jcsystem-unsupported-20261015-serial.log` (SHA-256 `d6f81b105cad5976c14879002191427e17d857d6d12c83c2fe0de482472945cc`).

## 2026-10-15 — Add RP2350 SHA-256 MessageDigest support

- Step/experiment: `alg-sha256-rp2350-20261015`
- Change: Added `MessageDigest.ALG_SHA_256` factory support using the existing `MessageDigest.OneShot` export, transient input buffering, and a native SHA-256 bridge. The Pico 2 ARM path calls the RP2350 `pico_sha256` accelerator; the PC emulator uses Mbed TLS so JCAlgTest can exercise the same native API. Updated the PC build wrapper to compile the emulator executable as well as the ROM mask, and added an assertion for the JCAlgTest SHA-256 supported-algorithm result.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc` passed. `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 60 --connect-timeout 60 --log artifacts/logs/jcalgtest-sha256-support-vpcd-20261015.log` passed `TEST:SUMMARY pass=9 fail=0`; SHA-256 support response was `1500FF0000000000000000000000000000000000000000000000009000`. `./tools/cycle choupi` rebuilt both architectures but its flash phase timed out after 120 seconds because no RP-series USB device appeared in this VM; no firmware was uploaded and no hardware validation was performed. An exploratory JCAlgTest performance APDU for `doFinal` returned `6F00` with `SecurityException` at `GETFIELD_S_THIS` in the applet path, so native digest execution is not yet verified by that APDU.
- Result: JCAlgTest 1.8.2 reports `ALG_SHA_256` supported in VPCD. The on-chip accelerator implementation compiles for the ARM target; SHA output correctness and hardware execution remain unverified. Build output contains existing Rust/C++ warnings from dependencies.
- Decision: Keep the support implementation; debug the separate JCAlgTest performance-call failure and rerun a bounded SHA known-answer test after USB passthrough is restored.
- Commit: pending
- Artifacts/logs: VPCD log `artifacts/logs/jcalgtest-sha256-support-vpcd-20261015.log` (SHA-256 `57466b2d9e41a369d1be4ab0199942e6cc480c4fcc5d7ed1daa7a95280eb6238`); physical flash attempt `artifacts/logs/cycle-choupi-20261015T132103Z.log` (SHA-256 `fe2431031ef112a6a2d8c0294c29c9bcf8b380b2e50976af86beccde606d204f`).

## 2026-10-15 — Record SHA-256 implementation commit

- Step/experiment: `alg-sha256-rp2350-20261015-commit-record`
- Change: Recorded the completed implementation commit in the session handoff; the SHA-256 implementation and validation record are in the preceding step.
- Validation: Documentation-only continuity update; no code validation rerun.
- Result: Implementation commit is `80855c3`; Java Card API subrepository commit is `79fd1f7`.
- Decision: Continue by fixing the JCAlgTest `doFinal` applet-path SecurityException, then validate a known-answer digest and flash when the device is exposed in BOOTSEL.
- Commit: `80855c3` (implementation commit)
- Artifacts/logs: See the preceding SHA-256 record.

## 2026-10-15 — Repair SHA-256 MessageDigest.doFinal initialization

- Step/experiment: `alg-sha256-dofinal-init-20261015`
- Change: Explicitly initialize `MessageDigest.OneShot.inputLength` to zero in its constructor. CHOUPI persists object fields separately and had not materialized Java's implicit zero value, so the first `doFinal()` path failed while reading the field. Added a JCAlgTest regression APDU that prepares SHA-256 `doFinal`, sends one input byte, and requires the `AA` success marker.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc` passed after the source fix. The bounded VPCD run passed `TEST:SUMMARY pass=9 fail=0`; the SHA-256 `doFinal` APDU returned `AA 9000`. The bounded hardware monitor `./tools/monitor --seconds 8` waited for USB discovery then failed with `ERROR: no USB serial device found`; physical APDU and serial validation were therefore not possible in this VM session.
- Result: The first-call SHA-256 `doFinal()` regression is fixed and covered in VPCD. Real-board behavior remains unverified. The VPCD run did not compare digest bytes against a known-answer vector; this step establishes successful execution only.
- Decision: Keep the fix. Complete the requested physical check after the Pico's post-boot USB interfaces are attached to this VM; inspect both the SHA APDU result and finite serial output.
- Commit: Java Card API `31d6ab4`; root regression test `1bb209c`.
- Artifacts/logs: `artifacts/logs/jcalgtest-sha256-dofinal-final-vpcd-20261015.log` (SHA-256 `8a2670a7a510d5240afaaefb0317cc8b2e6ebdacc093568ca9ece205349076a0`); no physical serial log was captured.


## 2026-10-15 — Retry physical SHA-256 validation in BOOTSEL

- Step/experiment: `physical-sha256-dofinal-retry-20261015`
- Change: No source code changed. Rebuilt both architectures and attempted to flash the ARM CHOUPI UF2 to the reported BOOTSEL Pico 2.
- Validation: `./tools/build arm` passed and produced `build/arm/pico2_choupi.uf2` (SHA-256 `241eaa1592459b8fc1f7d38e0576437c9797202606c7a720ce3e43f964d8ad06`); `./tools/build riscv` passed. `INFO_UF2.TXT` identified RP2350 BOOTSEL. `lsusb` reported libusb `-99`; picotool exited 250 with `ERROR: Failed to initialise libUSB`. The volume was mounted `ro`; the UF2 copy failed with `Read-only file system`. A bounded `timeout 5s pcsc_scan -r` timed out with PC/SC service unavailable.
- Result: The board was visible as a mounted RP2350 mass-storage volume, but not writable through this VM; `/dev/sdb` and `/dev/sdb1` were absent despite `lsblk` showing the device. No firmware transfer occurred and no physical SHA APDU or serial validation was performed.
- Decision: Stop hardware retries until VirtualBox USB passthrough exposes the BOOTSEL block device read/write. Then flash the ARM UF2 and inspect a finite serial log plus the SHA-256 `doFinal` APDU.
- Commit: `31d6ab4` (API fix) and `1bb209c` (hardware-test regression APDU under test; no source changes in this retry).
- Artifacts/logs: `artifacts/logs/physical-sha256-dofinal-retry-20261015.log` (SHA-256 `e9a7531e1dea9de35eab44b6e34a0f160aa8eb371c413bb5ddb870ef2da40608`).


## 2026-10-15 — Flash SHA-256 fix and inspect Pico 2 boot

- Step/experiment: `physical-sha256-dofinal-flash-boot-20261015`
- Change: No source code changed. Rechecked both builds, flashed the ARM CHOUPI UF2 through `./tools/flash`, then captured 18 seconds of serial output.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Elevated `lsusb -d 2e8a:000f` and picotool confirmed RP2350 BOOTSEL; flash wrapper reported `Loading into Flash: 100%`, device reboot, and `FLASH:PASS`. After reboot, elevated `lsusb -d 2e8a:0009` found Pico 2 CHOUPI CCID. `./tools/monitor --seconds 18` completed but emitted repeated `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`, `TEST:SUMMARY pass=0 fail=1`; CCID stayed at stage 2/endpoint 00. Bounded PC/SC scan listed only Virtual PCD readers.
- Result: Firmware was flashed and the board re-enumerated in application mode. Boot smoke is failing, and the physical SHA-256 APDU could not be sent because pcscd does not list the physical reader after USB re-enumeration.
- Decision: Have the user run `sudo -n /usr/local/sbin/pico2-pcsc-reader` to refresh pcscd access for current device 001:033. Once the physical reader appears, reinstall JCAlgTest 1.8.2 if needed and probe SHA-256 `doFinal`; separately investigate the boot-time applet restore `6F00`.
- Commit: `31d6ab4` (API fix) and `1bb209c` (regression APDU under hardware validation; no source changes in this flash step).
- Artifacts/logs: `artifacts/logs/physical-sha256-dofinal-serial-20261015.log` (SHA-256 `069f486067d58198cd15a90ced6fc11eb4c6f5a11e62f75c488d5dfeee705255`); flashed UF2 SHA-256 `241eaa1592459b8fc1f7d38e0576437c9797202606c7a720ce3e43f964d8ad06`.


## 2026-10-15 — Verify SHA-256 doFinal on physical JCAlgTest

- Step/experiment: `physical-sha256-dofinal-20261015`
- Change: Extended the PC/SC probe to prepare and invoke JCAlgTest 1.8.2 SHA-256 `MessageDigest.doFinal`, asserting its `AA` success marker. Reinstalled the supplied CAP after firmware reboot because the registered instance selected with `6F00`.
- Validation: `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported `PCSC:ACCESS_GRANTED node=/dev/bus/usb/001/033 service=pcscd.service`; plain `pcsc_scan -r` listed `Pico 2 CHOUPI CCID`. GlobalPlatformPro reloaded the CAP and LOAD/INSTALL returned `9000`. `./tools/probe-jc-algtest` passed physical SELECT (`9000`), GETVERSION `1.8.2_jc305`, JCSystem info (`9000`), SHA-256 prepare (`AA 9000`), SHA-256 `doFinal` (`AA 9000`), and expected unsupported RSA-SHA1 (`12 03`, `9000`). A finite post-APDU serial capture completed.
- Result: SHA-256 `doFinal` executes successfully on the real RP2350 with JCAlgTest 1.8.2; JCSystem APDU also passes in the same live card session. No digest known-answer comparison was performed. The boot-time serial check still fails repeatedly with `CHOUPI:STATUS sw=6F00` / `TEST:FAIL choupi_runtime`, despite the external APDU probe passing after CAP reinstall.
- Decision: Physical live-session `doFinal` verification passes. Continue separately with persistent applet restoration and startup health-check failure; add digest-output checking if correctness beyond call completion is needed.
- Commit: `33ad415` (physical APDU probe; implementation remains in API commit `31d6ab4`).
- Artifacts/logs: `artifacts/logs/physical-sha256-dofinal-apdu-20261015.log` (SHA-256 `de5fbf9ff643efe139f0378c077fa28a69a1b3f1057868d82c9dd1844351f41e`); `artifacts/logs/physical-sha256-dofinal-post-apdu-serial-20261015.log` (SHA-256 `88ad5f1a1f077c5ae499682728dafabe45f36aabdd170cf3145aaa4137d024ee`).

## 2026-10-15 — Persist the RP2350 Java Card filesystem

- Step/experiment: `rp2350-persistent-fs-snapshot-20261015`
- Change: Added two CRC-checked, generation-numbered 256 KiB flash snapshot slots at offsets `0x100000` and `0x140000`, below the existing CAP slot. Startup restores the newest valid overlay; filesystem writes are flushed after applet APDUs and install-state writes. A failed APDU snapshot returns `6F00`. Added host fake-flash coverage for restart restore, newest-generation choice, CRC fallback, and deletion persistence.
- Validation: `./tools/test-rp2350-fs-overlay` passed (`TEST:SUMMARY fail=0`); `./tools/build arm` and `./tools/build riscv` passed. ARM image end is `0x100b46fc`, below the filesystem slot at `0x10100000`. Physical validation was not run: `pcsc_scan -r` reported `SCardEstablishContext: Service not available` and flooded repeated release errors; `lsusb -d 2e8a:0009` reported libusb `-99`, and systemctl could not connect to the system bus.
- Result: Host tests establish crash-recoverable snapshots and corruption fallback. No flash latency, erase wear, or physical restart behavior has been measured; each dirty APDU currently writes a fixed 128 KiB payload.
- Decision: Keep the implementation for hardware validation. Once USB/PCSC access is restored, flash the ARM image and verify applet restore, JCSystem, SHA-256 `doFinal`, and bounded serial output across a power cycle.
- Commit: `56e625d`.
- Artifacts/logs: host test output in this session; no physical log.

## 2026-10-15 — Compact RP2350 filesystem snapshots and flash the board

- Step/experiment: `rp2350-fs-compact-snapshot-20261015`
- Change: Changed snapshot format to version 2, storing only occupied overlay records plus used overlay bytes. Each snapshot erases only the sectors needed for its payload, programs the payload first, and commits its CRC-checked header last. This reduces normal writes from the fixed 128 KiB overlay payload and 256 KiB slot erase.
- Validation: `./tools/test-rp2350-fs-overlay` passed all 19 checks. `./tools/build arm` and `./tools/build riscv` passed. `./tools/flash build/arm/pico2_choupi.uf2` passed through the repository wrapper after elevated host USB access was required; picotool reported 100% and reboot. `./tools/monitor --seconds 8` completed but repeatedly reported `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`, and `TEST:SUMMARY pass=0 fail=1`. A bounded `pcsc_scan -r` listed only the two Virtual PCD readers, so JCAlgTest could not be sent to the physical board.
- Result: The compact snapshot format passes host restart, generation selection, corruption fallback, and tombstone tests and is flashed. Physical boot health and snapshot restore remain unverified because the boot smoke test fails; static JCAlgTest measurements and SHA-256 results were not measured. UF2 SHA-256: `5955047a9d5016c93a8a6bc5d1054b822bdee90167e64ae40552e76c0c40188d`.
- Decision: Refresh PC/SC access for the newly enumerated CCID device, reinstall JCAlgTest 1.8.2 if needed, then run `OP_ALG_PERFORMANCE_STATIC`; inspect SHA-256 timing/results and fix any firmware failures. Investigate the boot-time `6F00` separately.
- Commit: `2ba182e`
- Artifacts/logs: Host test output and flash/monitor output were captured in this session but not archived.

## 2026-10-15 — Archive compact-snapshot boot log

- Step/experiment: `rp2350-fs-compact-boot-log-20261015`
- Change: Archived a second bounded serial capture from the flashed Pico to make its repeated startup failure available for diagnosis.
- Validation: `./tools/monitor --seconds 8` completed with status 0 and captured eight repetitions of `CHOUPI:STATUS sw=6F00` / `TEST:FAIL choupi_runtime`; the monitor status only indicates the capture ended on schedule, not a passing runtime.
- Result: Boot smoke remains failing after the compact snapshot firmware was flashed. Physical APDU validation is still blocked because the current PC/SC list has no Pico reader.
- Decision: Use this log to diagnose the boot-time SELECT failure. Continue the requested JCAlgTest run after PC/SC access is restored.
- Commit: `9dbf0b5`
- Artifacts/logs: `artifacts/logs/compact-fs-flash-boot-20261015.log` (SHA-256 `6c1a434a856af3f142c22452f8df5791275f6b5bb4f2aeab52a71129796c6a59`).

## 2026-10-15 — Check JCAlgTest static performance prerequisites

- Step/experiment: `jcalgtest-static-prereq-20261015`
- Change: Inspected the JCAlgTest 1.8.2 static MessageDigest benchmark path and attempted the focused VPCD APDU check while the physical PC/SC reader was unavailable.
- Validation: JCAlgTest `AlgPerformanceTest.perftest_class_MessageDigest` repeatedly invokes `doFinal()` on 256-byte buffers and returns only a success marker; the Java client measures elapsed time externally, so this run alone cannot establish digest correctness. Elevated `./tools/test-pc-vpcd-applet --passes 1 --connect-timeout 20 --apdu-timeout 10` failed with `TEST:FAIL timed out` before any APDU. `pcsc_scan -r` listed only two Virtual PCD readers; `pcscd -H` was denied (`Operation not permitted`), and direct `systemctl restart pcscd.service` timed out.
- Result: The requested `OP_ALG_PERFORMANCE_STATIC` run did not execute and produced no SHA-256 measurement. No new SHA-256 code defect was established; the physical JCAlgTest applet is currently inaccessible through PC/SC.
- Decision: Refresh the new physical USB node with the installed `/usr/local/sbin/pico2-pcsc-reader` helper, reinstall JCAlgTest 1.8.2 if needed, run the requested static benchmark, then check digest bytes separately with a known-answer probe.
- Commit: `d212af2`
- Artifacts/logs: `artifacts/logs/jcalgtest-sha256-vpcd-pending-physical-retry-20261015.log` (SHA-256 `6b04936b84bc17e27894a9aa19cd0ac72963312f473dd193533b2527d7c8ca7e`).

## 2026-10-16 — Fix repeated SHA-256 static performance calls

- Step/experiment: `jcalgtest-static-sha256-state-20261016`
- Change: Moved SHA-256 update, finalization, and reset state handling into a native bridge over the existing CLEAR_ON_RESET input array. The bridge stores the buffered length in the array tail, removes persistent scalar counter writes, and clears consumed input. Added the JCAlgTest 1.8.2 256-byte, 50-call `ALG_PERFORMANCE_STATIC` APDU to the focused VPCD regression.
- Validation: `./tools/build-pc` passed (`ROMMASK:PASS`); `./tools/build arm` passed (`BUILD:PASS target=choupi`); `./tools/build riscv` passed (`BUILD:PASS arch=riscv`). `timeout --signal=TERM 180s ./tools/env python3 tools/tests/pc_vpcd_applet_test.py --passes 1 --apdu-timeout 180 --connect-timeout 60 ...` passed all 11 checks. The static SHA-256 APDU returned `AA 9000` in 17.389 s; 50 × 256 = 12,800 input bytes, or about 736 bytes/s by whole-APDU host wall time. The official Java client selected `Virtual PCD 00 00`, but PC/SC reported `NO CARD`; CHOUPI logged `Connection refused` for `127.0.0.1:35963`, and `vsmartcard-vpcd` is absent. The static run therefore used the same JCAlgTest settings through the direct VPCD regression instead of the official PC/SC client. No physical flash or hardware benchmark was run.
- Result: Repeated static SHA-256 operations now complete without `F105` or `6F00`. The 17.389 s value is PC-emulator wall time, not an RP2350 measurement. JCAlgTest checks its success marker only; digest bytes were not known-answer checked.
- Decision: Keep the transient native state implementation. Enable a VPCD socket listener before rerunning the official Java client, then verify a digest known-answer and measure on the physical Pico when its reader is available.
- Commit: `fa8c05b` (root implementation and test); `c6c4da6` (Java Card API bridge).
- Artifacts/logs: `artifacts/logs/jcalgtest-sha-bridge-class-test-20261016.log`, `artifacts/logs/jcalgtest-sha-bridge-class-emulator-20261016.log`, `artifacts/logs/build-pc-sha-bridge-class-20261016.log`, `artifacts/logs/build-arm-sha-bridge-20261016.log`, `artifacts/logs/build-riscv-sha-bridge-20261016.log`, and `artifacts/logs/jcalgtest-official-vpcd-client-20261016.log`.

## 2026-10-16 — Record static benchmark reader selection and current access

- Step/experiment: `jcalgtest-static-reader-followup-20261016`
- Change: Updated the handoff with the actual JCAlgTest 1.8.2 selection behavior: `-simulator` chooses bundled jCardSim, `-cardname` is report metadata, and PC/SC devices are selected from a numbered list. Record the prior official-client disconnect and current PC/SC/USB state so the next run can target the reader by its listed name and index.
- Validation: `./tools/build arm` passed (`BUILD:PASS target=choupi`). A bounded `timeout --signal=TERM --kill-after=1s 3s pcsc_scan -r` ended with status 124 and `SCardEstablishContext: Service not available`; `lsusb -d 2e8a:0009` could not initialize libusb (`-99`). No fresh official-client or hardware test could run. The prior official static attempt logged setup and five SHA-256 measurement responses (`AA 9000`) before `SCARD_W_REMOVED_CARD`; its 50-call APDU did not complete. The focused PC VPCD test had already passed all 11 checks, including the complete 50-call static APDU (`AA 9000`, 17.389 s host wall time).
- Result: The implementation passes its focused complete static SHA-256 APDU, while the official-client end-to-end run and real-board measurement remain incomplete. The whole-APDU host timing is not a hardware throughput result; digest bytes have not been checked against a known answer.
- Decision: Once PC/SC and USB access are restored, select the real Pico by its displayed reader name/index and rerun `ALG_PERFORMANCE_STATIC`; add a digest known-answer probe separately.
- Commit: `836be14`.
- Artifacts/logs: `artifacts/logs/build-arm-jcalgtest-static-reader-20261016.log`, `artifacts/logs/jcalgtest-sha-bridge-class-test-20261016.log`, `artifacts/results/jcalgtest-static-vpcd-20261016/ALGTEST_log_1792133041312.log`.

## 2026-10-16 — Exercise JCAlgTest install parameters through the PC VM

- Step/experiment: `jcalgtest-install-params-c900-20261016`
- Change: Added `--install-params` to the focused VPCD regression runner. When a parameter is supplied, the emulator compares it with the actual Java install-entry frame: local 0 array reference/content, local 1 offset, local 2 signed byte length, and array size. A mismatch returns `6F00`. Updated TODO and handoff with the verified result.
- Validation: `python3 -m py_compile tools/tests/pc_vpcd_applet_test.py` passed; the runner help lists the new option. `./tools/build-pc` passed (49 existing Rust warnings and two packed-member C++ warnings); `./tools/build arm` passed with `BUILD:PASS target=choupi`. With localhost socket access enabled, the C900 integration run logged `VPCD:INSTALL_VM_ARGS status=PASS offset=0 length=2` and passed all 11 checks. GETVERSION returned `1.8.2_jc305`; JCSystem and SHA-256 checks returned `9000`; the 50-call static SHA-256 APDU returned `AA 9000` in 15.676 s.
- Result: The Java install-entry frame directly matches offset 0, `bLength` 2, and bytes `C9 00`; applet installation and subsequent process APDUs succeed. The static SHA timing is PC emulator wall time, not hardware throughput. No physical reader was available; no flash was attempted.
- Decision: Keep this VM-entry assertion in the PC regression. Continue lifecycle golden coverage, GP card-manager behavior, and remaining AC checks.
- Commit: `822b67e`.
- Artifacts/logs: `artifacts/logs/build-pc-install-args-assert-20261016.log`, `artifacts/logs/build-arm-install-args-assert-20261016.log`, `artifacts/logs/vpcd-install-params-vmcheck-test-20261016.log`, and `artifacts/logs/vpcd-install-params-vmcheck-20261016.log`.

## 2026-10-16 — Select JCAlgTest target by PC/SC reader name

- Step/experiment: `jcalgtest-reader-selector-20261016`
- Change: Added `tools/run-jcalgtest` with explicit `--reader-name NAME` and `--simulator` modes. A Java SmartcardIO helper resolves exact PC/SC names to the same one-based list index used by JCAlgTest 1.8.2. In reader mode the chosen name fills `-cardname` unless overridden; simulator mode invokes the official `-simulator` option. The wrapper creates a supplied `-outpath` directory and uses a finite timeout.
- Validation: `bash -n tools/run-jcalgtest`, wrapper help, and Java compilation passed. An unknown exact reader name failed closed with exit 4 and listed the Java-visible readers (none). The official `ALG_SUPPORT_EXTENDED` client completed in simulator mode with exit 0, generated a 4,595-line CSV, and logged the expected jCardSim `NotImplementedException`/algorithm failures. Named PC/SC selection was not exercised because Java SmartcardIO currently sees no readers; no token, APDU, or firmware was accessed.
- Result: Both official-client target modes are now directly selectable from the launcher. Simulator execution is confirmed; the real/virtual PC/SC path remains pending host reader visibility. This run validates selection and launch only, not full algorithm correctness.
- Decision: Use `tools/run-jcalgtest --reader-name '<exact pcsc_scan reader name>' -op ALG_PERFORMANCE_STATIC -fresh -outpath <directory>` for a real/virtual card, or `tools/run-jcalgtest --simulator ...` for bundled jCardSim. Retry once Java SmartcardIO sees the target reader.
- Commit: `9ff15ec17ab3e552d1bfda66da4bf75922bd3638`.
- Artifacts/logs: `artifacts/logs/jcalgtest-reader-selector-simulator-20261016.log` (SHA-256 `c68e2bc083e95af186c7032c958969ab6ba9e034af6d439fdbbb279b51aafd25`); `artifacts/results/jcalgtest-reader-selector-simulator-20261016/`.

## 2026-10-16 — Run JCAlgTest static mode through the selector

- Step/experiment: `jcalgtest-static-selector-simulator-20261016`
- Change: Exercised the new official-client launcher in `ALG_PERFORMANCE_STATIC` mode through bundled jCardSim and archived the complete output up to timeout. Updated the handoff with measured limits and the prior VPCD interruption state.
- Validation: `timeout 240s tools/run-jcalgtest --simulator -op ALG_PERFORMANCE_STATIC -fresh -outpath artifacts/results/jcalgtest-reader-selector-static-simulator-20261016` reached and completed SHA-256’s 250 × 256-byte sample section, then exited 124 during the later RSA sweep. Simulator SHA-256 reported 0.05 ms/op average, -0.02 ms/op minimum, with raw sample deltas 14.20, 0.20, -0.80, 0.20, -0.80 ms. The earlier official PC/SC VPCD run received `AA 9000` in SHA-256 baseline samples but lost its card before measured samples. No reader was visible to Java SmartcardIO in this session, so no hardware benchmark was run.
- Result: SHA-256 calls execute and report success. Simulator timing includes sub-resolution/negative deltas and is not a sensible throughput estimate. The focused CHOUPI host VPCD 50-call APDU previously returned `AA 9000` in 15.676 s, also host-emulator timing, not Pico throughput. No firmware defect is established by these timings; digest bytes still need a known-answer test.
- Decision: Do not change SHA based on simulator timing noise. Retry static mode against the exact physical Pico reader after Java SmartcardIO sees it; do not classify expected unsupported algorithms as bugs.
- Commit: `8dd50ebdb0c08d7e30a553f1e0e8866ea6cf607e`.
- Artifacts/logs: `artifacts/logs/jcalgtest-reader-selector-static-simulator-20261016.log` (SHA-256 `9e20a5e357b29b258d1d192ddb3f6cecb25bf9a60ca235f203ef09692eca31a0`); `artifacts/results/jcalgtest-reader-selector-static-simulator-20261016/`.

## 2026-10-16 — Verify GlobalPlatform GET STATUS applet entry encoding

- Step/experiment: `gp-get-status-registry-tlv-20261016`
- Change: Factored the installed-applet E3 registry entry used by CCID GET STATUS into a bounds-checked encoder. The byte layout is unchanged; the supplied JCAlgTest applet AID produces a 32-byte entry with a matching 30-byte outer value. Added exact-byte and invalid-input coverage. Preserved unrelated existing changes in `src/ccid_device.cpp` and staged only the include and GET STATUS encoder hunk.
- Validation: `./tools/test-gp-registry` passed 7 checks (`TEST:SUMMARY pass=7 fail=0`). `./tools/build arm` passed both `BUILD:PASS arch=arm` and `BUILD:PASS target=choupi`; no warnings were emitted in the captured build output. No RISC-V build or hardware APDU/flash was run. Hardware validation was skipped because Java SmartcardIO has no visible reader here and the required dual-architecture flash gate cannot be satisfied while following the ARM-only scope.
- Result: The installed applet registry response helper emits the exact nested TLV bytes and rejects short output capacity and invalid instance AID length. This validates the encoding helper, not the embedded PC/SC transaction; GET STATUS lifecycle accuracy, DELETE, and GET DATA remain incomplete.
- Decision: Keep the response wire representation unchanged. Continue card-manager lifecycle behavior and host-accessible APDU coverage; do not flash this build until the repository gate is met.
- Commit: `7335e217e092fda324d44657c60817eabecadbbd`.
- Artifacts/logs: `artifacts/logs/gp-registry-tlv-test-20261016.log` (SHA-256 `d1b2057bb43770a76617c72a943d1eac449a3ec8ee020e6e58c4572e7184d0a4`); `artifacts/logs/build-arm-gp-registry-tlv-20261016.log` (SHA-256 `ef6de0d8ed9bca880d446b9353e396063980920f08e1cef65100b8ec9e714b4d`).

## 2026-10-16 — Report package AID with GP GET STATUS tag C4

- Step/experiment: `gp-get-status-load-file-aid-tag-20261016`
- Change: Corrected the applet registry entry to encode its package/Executable Load File AID as `C4`. `CC` is the distinct associated Security Domain AID tag. Updated the exact response-byte regression accordingly.
- Validation: `./tools/test-gp-registry` passed all 7 checks. `./tools/build arm` passed both ARM smoke and CHOUPI firmware images. No RISC-V build, flash, or hardware APDU was performed.
- Result: GET STATUS no longer labels the JCAlgTest package AID as an associated Security Domain. The official GlobalPlatform Card Specification 2.2.1 registry data table defines `C4` as the application's Executable Load File AID and `CC` as the associated Security Domain AID. End-to-end GPPro/physical verification is still unavailable; GET STATUS search/session semantics and DELETE/GET DATA remain incomplete.
- Decision: Keep C4 and continue with GET STATUS query/continuation behavior before implementing more card data.
- Commit: `afb6c5ed5833110ca7d6e1145b96d8cbdc6083c5`.
- Artifacts/logs: `artifacts/logs/gp-registry-load-file-aid-test-20261016.log` (SHA-256 `554c2ec5e9d8b1ce795036b9a84a69a1178b3ad7fa8795dd920d0b59c4807a41`); `artifacts/logs/build-arm-gp-registry-load-file-aid-20261016.log` (SHA-256 `3f4a64b2a7335709e930163ad83167889a2425e8c41afc97b1887d2bb2ed63db`).

## 2026-10-16 — Implement GET STATUS categories and AID search

- Step/experiment: `gp-get-status-categories-search-20261016`
- Change: Added GET STATUS handling for ISD, applications, executable load files, and load files with modules. Queries require a leading `4F` AID qualifier and support wildcard, exact, and RID-prefix matching; well-formed trailing qualifiers are ignored. Query records are built from loaded CAP metadata, including the actual package AID. Added first/next request validation and reset its state on card power and SELECT.
- Validation: `./tools/test-gp-registry` passed 21 checks (`TEST:SUMMARY pass=21 fail=0`). `./tools/build arm` passed both `BUILD:PASS arch=arm` and `BUILD:PASS target=choupi` without warnings. No RISC-V build, physical APDU, or flash was performed; RISC-V is out of scope and the dual-build flash gate therefore remains unmet.
- Result: GET STATUS encoders and search parser pass focused host checks; the ARM images compile. Dispatcher behavior has not yet been exercised end to end through GPPro or on hardware. No hardware metric was measured.
- Decision: Keep this implementation and add host-level APDU coverage before treating card-manager query behavior as verified. Continue with truthful GET DATA behavior and transaction-safe DELETE.
- Commit: `7b55114`.
- Artifacts/logs: `artifacts/logs/gp-status-search-test-20261016.log` (SHA-256 `eeb0993016685ca0564b2bbfeb9ddf1587f47c71a4595aa1e608126870a341ab`); `artifacts/logs/build-arm-gp-status-search-20261016.log` (SHA-256 `9334763cc20ce770d8a29a75435dfca4da870bb3c3153068afec2617b55b08e7`).

## 2026-10-16 — Remove stale hardcoded package AID

- Step/experiment: `gp-get-status-package-aid-cleanup-20261016`
- Change: Removed the old compile-time JCAlgTest package AID constant. GET STATUS now reports the package AID from the loaded CAP metadata.
- Validation: The preceding `./tools/build arm` passed both ARM images after this source change; `git diff --cached --check` passed for the scoped cleanup. No hardware check was performed.
- Result: No remaining GET STATUS package-AID constant can disagree with the currently loaded CAP metadata.
- Decision: Keep metadata-driven package reporting; continue with APDU-level coverage.
- Commit: `7970700`.
- Artifacts/logs: Same ARM build log as the preceding step: `artifacts/logs/build-arm-gp-status-search-20261016.log` (SHA-256 `9334763cc20ce770d8a29a75435dfca4da870bb3c3153068afec2617b55b08e7`).

## 2026-10-16 — Honor ISD GET STATUS search semantics

- Step/experiment: `gp-get-status-isd-search-20261016`
- Change: Corrected GET STATUS `P1=80` to return only the ISD and ignore the AID search data, as required by GlobalPlatform Card Specification 2.2.1 section 11.4.2.1. Extracted the bounded ISD E3 encoder and added exact-byte and capacity checks.
- Validation: `./tools/test-gp-registry` passed 26 checks (`TEST:SUMMARY pass=26 fail=0`). `./tools/build arm` passed both ARM images with no warnings. No RISC-V build, physical APDU, or flash was performed.
- Result: ISD GET STATUS no longer rejects malformed or nonmatching AID qualifiers; the P1=80 behavior is now distinct from application/package search. No end-to-end card-manager measurement was made.
- Decision: Continue with host-level dispatcher coverage, then truthful GET DATA and GP DELETE behavior.
- Commit: `9c81875`.
- Artifacts/logs: `artifacts/logs/gp-status-isd-test-20261016.log` (SHA-256 `a50088a03fd5229f7db8c495c1bb43f5f1809e36a1814e2a2f38336737e47e2b`); `artifacts/logs/build-arm-gp-status-isd-20261016.log` (SHA-256 `2f5b1775618ac41348bd3b76240afb0f4dd8b192db8a468983bdaebccca3b6dc`).

## 2026-10-16 — Test GET STATUS response categories

- Step/experiment: `gp-get-status-response-builder-20261016`
- Change: Extracted first/all GET STATUS response construction into a platform-independent registry helper used by the CCID dispatcher. Added behavior checks for ISD qualifier ignoring, applet/package/module results, exact response bytes, malformed/missing AID search, empty registries, and unsupported P1 values.
- Validation: `./tools/test-gp-registry` passed 39 checks (`TEST:SUMMARY pass=39 fail=0`). `./tools/build arm` passed both ARM smoke and CHOUPI firmware images without warnings. No RISC-V build, physical APDU, or flash was performed.
- Result: All four first/all response categories now have focused helper-level tests, but there is still no end-to-end CCID dispatcher or GPPro APDU run. No hardware result was measured.
- Decision: Continue with explicit session/continuation tests, then evaluate truthful GET DATA behavior and GP DELETE.
- Commit: `5efa359`.
- Artifacts/logs: `artifacts/logs/gp-status-query-test-20261016.log` (SHA-256 `845e03907fe4d605567c3ff8154a2555663715f5f4a5f4bf2444fd77d394ca40`); `artifacts/logs/build-arm-gp-status-query-20261016.log` (SHA-256 `70a48020d952ff42ecdb1a06b4baf920f07761391947c0f6814183ecd66afa6c`).

## 2026-10-16 — Validate GET STATUS next-occurrence requests

- Step/experiment: `gp-get-status-next-validation-20261016`
- Change: Replaced remembered-P1 equality checking with explicit GET NEXT validation: a prior first/all query must exist in the same card session; ISD-only P1 `80` cannot continue; supported non-ISD categories require a valid leading `4F` AID search. Added focused policy checks and retained the existing power/SELECT session resets.
- Validation: `./tools/test-gp-registry` passed 44 checks (`TEST:SUMMARY pass=44 fail=0`); `./tools/build arm` passed both ARM images. `git diff --cached --check` passed before commit. No physical APDU, flash, or RISC-V build was run; RISC-V remains out of scope and the dual-architecture flash gate is unmet.
- Result: GET NEXT returns the policy status from a portable validator and no longer incorrectly requires the next P1 to match the previous first/all P1. The single-package registry still has no additional record to return. Hardware behavior remains unverified.
- Decision: Keep this session validation and continue host APDU coverage, then truthful GET DATA and transactional GP DELETE.
- Commit: `b9810e7`.
- Artifacts/logs: `artifacts/logs/gp-status-session-test-20261016.log` (SHA-256 `32b6f462fe2bb2326002c121bee12e23861c3f7d504ab118c06efa91cd5e72ab`); `artifacts/logs/build-arm-gp-status-session-20261016.log` (SHA-256 `827b202dade3226dd02f80824a128a57638cdbc2f36fcce39a38f53d9f8bfc41`).

## 2026-10-16 — Evaluate static SHA-256 benchmark target availability

- Step/experiment: `jcalgtest-static-target-evaluation-20261016`
- Change: Retried JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` against the exact Pico CCID reader name through `tools/run-jcalgtest`; confirmed the launcher fails closed when Java SmartcardIO cannot see the target. Reviewed the completed simulator output and prior CHOUPI PC-emulator APDU run.
- Validation: `JCALGTEST_TIMEOUT_SECONDS=15 tools/run-jcalgtest --reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]' -op ALG_PERFORMANCE_STATIC -fresh -outpath /tmp/jcalgtest-static-physical` exited 4 with `PC/SC reader not found`; Java SmartcardIO listed no readers. A bounded `pcsc_scan -r` also showed `SCardEstablishContext: Service not available` and began an invalid-handle flood; output was redirected and removed. No APDU or hardware access occurred. Archived simulator run completed SHA-256's 250 × 256-byte sample section; raw sample times were 14.20, 0.20, -0.80, 0.20, and -0.80 ms, yielding 0.05 ms/op average and -0.02 ms/op minimum. The prior CHOUPI PC-emulator 50-call SHA-256 APDU returned `AA 9000` in 17.389 s, which is not device performance.
- Result: The simulator timer is below its useful resolution and the host-emulator latency cannot establish Pico throughput. JCAlgTest reports only `AA 9000`, not digest bytes, so these results do not validate SHA-256 correctness or justify a firmware change. Physical timing remains unavailable until Java sees the Pico reader.
- Decision: Keep the current SHA-256 implementation unchanged pending a real-board benchmark and independent known-answer digest check. Use `tools/run-jcalgtest --reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]' -op ALG_PERFORMANCE_STATIC -fresh -outpath <directory>` for the physical target, or `tools/run-jcalgtest --simulator ...` for jCardSim. Do not retry PC/SC while the service is unavailable.
- Commit: `c13569c` (evidence, QKB, and handoff record).
- Artifacts/logs: Existing simulator log `artifacts/logs/jcalgtest-reader-selector-static-simulator-20261016.log` (SHA-256 `9e20a5e357b29b258d1d192ddb3f6cecb25bf9a60ca235f203ef09692eca31a0`); earlier CHOUPI host-emulator log `artifacts/logs/jcalgtest-sha-bridge-class-test-20261016.log`; physical-selector failure was temporary and removed after capture.

## 2026-10-16 — Revalidate JCAlgTest applet execution through host VPCD

- Step/experiment: `jcalgtest-host-vpcd-current-worktree-20261016`
- Change: Rebuilt ARM for the current worktree and reran one clean host VPCD applet integration pass to revalidate SELECT, process APDUs, JCSystem, and SHA-256 following the latest VM/lifecycle changes. Updated TODO and handoff to replace stale first-process-APDU failure notes with the current evidence.
- Validation: `./tools/build arm` passed both `pico2_jcvm_smoke.uf2` and `pico2_choupi.uf2` without warnings. The loopback VPCD regression initially could not bind under the default sandbox (`Operation not permitted`); rerunning with the required local-socket permission passed all 11 checks (`TEST:SUMMARY pass=11 fail=0`). Applet APDUs passed ATR, SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, supported-algorithm probes, SHA-256 prepare/doFinal, 50-call static SHA-256, and AES/RSA/random/CRC16 support probes. No RISC-V build, physical APDU, or flash was run.
- Result: One fresh PC-emulator applet pass completed in 72.597 s; the 50-call 256-byte static SHA-256 APDU took 15.455 s. These are host-emulator measurements only. The official JCAlgTest static path returns only `AA 9000`; digest bytes remain unchecked. The pass confirms these `process(APDU)` paths work in the host VM, not hardware persistence or complete lifecycle coverage.
- Decision: Keep the applet runtime changes and extend correctness coverage with a digest known-answer route; separately resolve physical PC/SC visibility and verify RP2350 persistence after power cycle. Do not flash while the repository’s two-architecture gate is unmet under the ARM-only target scope.
- Commit: pending metadata commit.
- Artifacts/logs: `artifacts/logs/pc-vpcd-continuation-audit-run-20261016.log` (SHA-256 `c25786b3fffccb050e5c71e2366d1971779656f077b418ca7fc7bf73be001c25`); `artifacts/logs/pc-vpcd-continuation-audit-20261016.log` (SHA-256 `2fb37f0f9daf3ac3952ce635e2db9856fc4df6b30f8bf37527e0256c1cc391a7`); `artifacts/logs/build-arm-continuation-audit-20261016.log` (SHA-256 `56a55b6c1a15d4b1442d1ca4a734d69ebc4b66fdb3a3de3acd9b51305e825fe7`).

## 2026-10-16 — Verify applet restoration across fresh host VPCD processes

- Step/experiment: `jcalgtest-host-vpcd-persistence-two-pass-20261016`
- Change: Repeated the JCAlgTest host VPCD applet regression for two fresh emulator processes using the same persisted applet image. Updated TODO and handoff to distinguish this host restart evidence from the still-unverified RP2350 power cycle.
- Validation: `timeout --signal=TERM --kill-after=5s 300s ./tools/test-pc-vpcd-applet --passes 2 --connect-timeout 30 --apdu-timeout 60 --log artifacts/logs/pc-vpcd-persistence-pass-20261016.log` passed (`TEST:SUMMARY pass=22 fail=0`). Both processes completed ATR, SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, support probes, SHA-256 prepare/doFinal, the 50-call static APDU, and AES/RSA/random/CRC16 probes. Per-pass total times were 66.980 s and 68.283 s; static SHA APDU times were 15.510 s and 15.599 s. Loopback socket access was required. No RISC-V build, physical power cycle, physical APDU, or flash was performed.
- Result: Host VM restart and applet restore are reproducible for these applet interactions. The same test does not prove the Pico flash snapshot or applet state survives a real board power cycle. Timings are PC-emulator measurements and digest bytes were not checked.
- Decision: Keep the host persistence result as integration evidence and continue with a SHA-256 known-answer route plus board persistence once device access is restored.
- Commit: pending metadata commit.
- Artifacts/logs: `artifacts/logs/pc-vpcd-persistence-run-20261016.log` (SHA-256 `9c1640df4653f9ebfbee683cd546a3b512d447d4c4b54ccfbf9cdaff44196188`); `artifacts/logs/pc-vpcd-persistence-pass-20261016.log` (SHA-256 `18aff547718ebe03b4760a8bef346151ab4470eae1e73b671b2fda49284dac5c`).

## 2026-10-16 — Expose ARM JCRE startup failure details

- Step/experiment: `arm-jcre-startup-failure-diagnostic-20261016`
- Change: When the initial firmware JCRE runtime returns a non-`9000` status, print the interpreter's exception ID, opcode, PC, stack depth, and rolling opcode/depth history as `CHOUPI:VM_RUNTIME_FAILURE`. This makes the existing boot-time `6F00` failure actionable in the next serial capture. Updated TODO and handoff to state that the latest flashed snapshot image fails boot smoke and that this diagnostic still needs a hardware run.
- Validation: `./tools/build arm` passed `pico2_jcvm_smoke.uf2` and `pico2_choupi.uf2` with no warnings (`BUILD:PASS arch=arm`, `BUILD:PASS target=choupi`). The CHOUPI image ends at `0x100b5818`, below the persistent-store region. No RISC-V build, flash, or hardware serial check was run; the user's ARM-only instruction excludes RISC-V and the repository dual-build flash gate is therefore unmet.
- Result: The next authorized ARM startup run will distinguish interpreter exceptions from generic `6F00` and retain the fault location/history. Firmware behavior is otherwise unchanged; the boot failure is not fixed or verified.
- Decision: Use the new diagnostic in a bounded serial capture after the build-gated hardware path is available; fix the reported runtime failure before claiming physical integration.
- Commit: `437365d`.
- Artifacts/logs: `artifacts/logs/build-arm-runtime-diagnostic-20261016.log` (SHA-256 `56f72483fc1a6cbfa919cd46089e40d67f290762c2002c938385de3613057cbb`); previous failing serial evidence `artifacts/logs/arm-resume-boot-20261016.log`.

## 2026-10-16 — Select named reader for JCAlgTest static performance

- Step/experiment: `jcalgtest-static-target-selection-20261016`
- Change: Used the official-client launcher’s exact `--reader-name` selector for the Pico and `--simulator` mode for bundled jCardSim. No runtime implementation changed.
- Validation: `tools/run-jcalgtest --reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]' -op ALG_PERFORMANCE_STATIC -fresh ...` exited 4 before APDUs because Java SmartcardIO listed zero readers. The bundled simulator ran the official static mode through SHA-256 doFinal/update/reset and into SHA-384, then was interrupted during the later sweep (exit 130). All observed SHA APDUs returned `AA 9000`.
- Result: Simulator SHA-256 doFinal measured 0.02 ms/op average with -0.11 ms/op minimum; update measured 0.18 ms/op and reset 0.03 ms/op. Negative baseline-subtracted timing confirms the simulator timer is too coarse for useful throughput conclusions. The applet returns status only, so no digest known-answer check occurred. Physical throughput remains unmeasured.
- Decision: Leave SHA implementation unchanged on these data. Retry with the exact Pico reader name when Java SmartcardIO exposes the reader, and use an independent digest known-answer check.
- Commit: pending
- Artifacts/logs: `artifacts/logs/jcalgtest-static-pico2-reader-20261016.log` (SHA-256 `37ed79c092933ee6c76b01cd5fda7858cb9cb854129e0994c7188399ea205ebc`); `artifacts/logs/jcalgtest-static-simulator-20261016.log` (SHA-256 `749bb1ffc17077898c7efb234e80a12df3400eb4627f4eba8fcf1340fd63f137`); partial CSV SHA-256 `0c0b38ccf8a87acb874d1584b5b2b589447b24930d5df7d19f6986c357ffcbbc`.

## 2026-10-16 — Add strict GP DELETE request parser

- Step/experiment: `gp-delete-request-parser-20261016`
- Change: Added a platform-independent parser for one GlobalPlatform DELETE AID qualifier (`4F`), supported P1/P2 values, and the related-object flag. Added a focused C++ test wrapper covering valid AIDs, flags, malformed/truncated TLVs, reserved bits, and non-minimal lengths.
- Validation: `./tools/test-gp-delete` passed 11 checks with `-Wall -Wextra -Werror`; `./tools/build arm` passed the smoke and CHOUPI ARM images without warnings. The parser is not yet connected to the authenticated card-manager dispatch, so no APDU or persistent deletion was exercised.
- Result: Malformed and unsupported command encodings are rejected before any state mutation; parsed AID remains a bounded view into caller data.
- Decision: Keep GP DELETE open. Next integrate the parser with secure-channel dispatch and implement deletion semantics that remain consistent across restart and GET STATUS.
- Commit: pending
- Artifacts/logs: Focused test output `TEST:SUMMARY pass=11 fail=0`; ARM image outputs `build/arm/pico2_jcvm_smoke.uf2` and `build/arm/pico2_choupi.uf2`.

## 2026-10-16 — Confirm JCAlgTest static simulator run completion

- Step/experiment: `jcalgtest-static-simulator-completion-20261016`
- Change: Followed up the named-reader attempt after the Java child continued beyond the interrupted tool session. The final client log footer and CSV confirm the official `ALG_PERFORMANCE_STATIC` run completed; the test was not rerun.
- Validation: Pico exact-name selection still exits 4 with an empty Java SmartcardIO reader list. The jCardSim report contains 1,900 lines and 132 measured-operation sections. It records 290 `NO_SUCH_ALGORITHM`, 204 `CANT_BE_MEASURED`, and 66 `ILLEGAL_VALUE` outcomes. SHA-256 doFinal/update/reset APDUs returned `AA 9000`.
- Result: jCardSim SHA-256 doFinal is reported as 0.02 ms/op average, -0.11 ms/op minimum, 0.51 ms/op maximum; update is 0.18 ms/op average; reset is 0.03 ms/op average. Negative baseline-subtracted time is timer noise. The client does not return digest bytes, and no hardware throughput or SHA known-answer was measured.
- Decision: Do not change the SHA implementation from this simulator output. The real Pico static benchmark remains outstanding until Java can see its PC/SC reader. Algorithm absence in this report is not by itself a firmware defect.
- Commit: pending
- Artifacts/logs: Final console log SHA-256 `0581e4756cb6de6ff34b783a37735b41031c1af6e5ce8b0196a13a35db9909ab`; client log SHA-256 `ac5590b26b7e2f2c6a8a27adc78b0e6d187be4a5e8354b4712634308b124482a`; CSV SHA-256 `24a4a417dc8d74779c2444b71d565ec67a33fc48bb0e9a6dcb3ddbbe6fd5cca0`.

## GP DELETE parser commit metadata

- The strict request parser and 11-check test step recorded above was committed as `36ebf1e`.

## 2026-10-16 — Integrate authenticated GP DELETE lifecycle handling

- Step/experiment: `gp-delete-lifecycle-20261016`
- Change: Connected `INS=E4` to the authenticated card-manager dispatcher. Implemented instance and package deletion status handling, a reserved filesystem lifecycle marker for restart restoration, transaction-aware persistent-tag erasure, package static-state cleanup, and erase of the reserved CAP slot after package deletion. Successful CAP LOAD/INSTALL clears stale markers.
- Validation: `./tools/test-gp-delete` passed 11 checks; `./tools/build arm` passed both ARM images; `./tools/build-pc` passed and ROMMASK generation completed. The PC-only VPCD applet test passed `TEST:SUMMARY pass=11 fail=0` in 66.365 s after a clean host rebuild. It exercised the normal applet path, not GP DELETE. The PC OS build emits 49 existing Rust warnings. No RISC-V build or flash was run.
- Result: Normal CAP load, restore, SELECT, JCSystem, support probes, SHA-256 prepare/doFinal, and static SHA APDU continue to pass on the host emulator. No SCP03-authenticated DELETE APDU or post-delete restart test has exercised the new path.
- Decision: Keep DELETE and card-manager criteria open until authenticated instance/package DELETE, failure statuses, and GET STATUS after restart are tested. Java SmartcardIO reports no readers, so physical validation remains unavailable here.
- Commit: pending
- Artifacts/logs: `artifacts/logs/build-arm-gp-delete-20261016.log` (SHA-256 `17a6cb5bdc47a2b83cce2c6c52058fc0750c1c4743a87a7d7f04ec8ad5e6b78e`); `artifacts/logs/build-pc-gp-delete-final-20261016.log` (SHA-256 `031952a9caed256cb031f1ca2be1f531567faf6c6867df7134f652082771679a`); `artifacts/logs/pc-vpcd-gp-delete-run-20261016.log` (SHA-256 `f59a665bd0d3bf318f9a87a55df8ff7ce7231dd9965a3b7eb903848b7969a8f7`); emulator log `artifacts/logs/pc-vpcd-gp-delete-20261016.log` (SHA-256 `32960ddd67f8ec20d5e065871560883458c7827e5697a46040bfe8c549d4d0f9`).

## GP DELETE lifecycle commit metadata

- The authenticated dispatcher, persistent state updates, strict parser integration, and ARM/PC/VPCD evidence recorded above are committed as `4f66267`.
## 2026-10-16 — Repeat ARM and applet host validation

- Step/experiment: `arm-vpcd-regression-repeat`
- Change: No firmware source changed; reran the ARM wrapper build and the current JCAlgTest 1.8.2 host VPCD integration to establish the checkout's current baseline. Updated TODO and session handoff with this evidence and its limits.
- Validation: `./tools/build arm` passed for `pico2_jcvm_smoke.uf2` and `pico2_choupi.uf2`. `./tools/test-pc-vpcd-applet --passes 2` passed `TEST:SUMMARY pass=22 fail=0`; SELECT, GETVERSION, JCSystem info, SHA-256 prepare/doFinal/static calls and AES/RSA/random/CRC support probes passed across two fresh emulator processes.
- Result: The 50-call static SHA APDU took 13.985 s and 13.487 s in the host emulator. This is not physical Pico performance or a digest known-answer check. The named PC/SC reader remains unavailable to Java SmartcardIO; no APDU was sent to hardware.
- Decision: Treat the ARM build and host applet path as passing, while keeping physical verification, digest correctness, and JCSystem transaction behavior open. Continue with transaction lifecycle and Java exception mapping.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/build-arm-validation-20261016.log` (SHA-256 `0b8cfc50e821a2a925e2c7dcb722257e8f30db826058079987a598df1655dd5e`); `artifacts/logs/host-vpcd-validation-20261016-console.log` (SHA-256 `6842877564d40fd7341a35af0dd86c58b1ce8feb6e64c246f80a1b61e4233a8a`); `artifacts/logs/host-vpcd-validation-20261016.log` (SHA-256 `7619bf6bb9290afb673518ef3c9aabebace219714db4a92fa7d56182af50acbf`).

## Current ARM validation commit metadata

- The ARM rebuild and two-pass host applet regression record above was committed as `b33c1eb`.

## 2026-10-16 — Implement JCSystem transaction lifecycle

- Step/experiment: `jcsystem-persistent-transaction-api`
- Change: Connected `JCSystem.beginTransaction`, `commitTransaction`, `abortTransaction`, and `getTransactionDepth` to the persistent-write snapshot backend. Java wrappers map duplicate begin, commit/abort without an active transaction, and rollback failure to `TransactionException` reason codes. Fixed `Util.arrayCopy`, `arrayFill`, and `setShort` transaction ownership logic and validate arguments before opening a transaction.
- Validation: `./tools/test-persistent-transaction` passed 24 checks, including inactive/active depth, nested begin rejection, commit, rollback, created-record removal, and write-failure recovery. `./tools/build arm` passed ROM-mask/API generation and both ARM images. Two fresh host VPCD emulator passes completed `TEST:SUMMARY pass=22 fail=0`, including JCSystem info, SHA-256, and support APDUs. No RISC-V build or hardware flash was performed.
- Result: Host applet operations continue through the revised Java utility wrappers. Static 50-call SHA APDUs took 14.566 s and 13.442 s in the host emulator. No Java APDU directly exercised transaction exception reasons. Commit-capacity queries remain zero, and the snapshot journal is volatile RAM state, so power-loss transaction atomicity is not established.
- Decision: Keep transaction capacity, Java exception-path coverage, and crash-safe persistent transactions open. Continue with a JCSystem transaction applet test and then implement capacity/recovery work; do not claim power-cycle or physical behavior from host results.
- Commit: Java API submodule `b757a0a`; root implementation commit recorded in the following metadata entry.
- Artifacts/logs: `artifacts/logs/build-arm-jcsystem-transaction-20261016.log` (SHA-256 `2db89bbbc9bf216f876dada393af6acf2317a111df6ce67ed9b1c67de020871b`); `artifacts/logs/persistent-transaction-jcsystem-20261016.log` (SHA-256 `733a7c09076c4891f673dbdee18924cbfed03360be452c1a7b93a1d19b60500f`); `artifacts/logs/host-vpcd-jcsystem-transaction-final-console-20261016.log` (SHA-256 `16b188c22ae97c371e0153531b13630773b07ce4e44d502dbcf1666a4424be13`); `artifacts/logs/host-vpcd-jcsystem-transaction-final-20261016.log` (SHA-256 `1513da10cebf0f7bda56abd759f042e40a3291b92b78e741bd8ba044940a8824`).

## JCSystem transaction implementation commit metadata

- The root implementation, 24-check rollback test, ARM build, two-pass host applet regression, and progress/handoff records above were committed as `e39dfdd`. The Java Card API source changes are separately committed in its nested repository as `b757a0a`.

## 2026-10-16 — Add host filesystem interrupted-write recovery coverage

- Step/experiment: `fs-single-record-power-cut-test`
- Change: Added test-only host flash failure injection after a selected count of programmed 32-bit words. Added RAII cleanup for flash unlock and sector-range locks after unwinding, modernized the OS host test entry points for Rust 1.99, and added a filesystem replacement test that simulates a cut at every programmed-word boundary and reconstructs the filesystem.
- Validation: `./tools/env cargo test --manifest-path choupi/os/Cargo.toml --no-default-features --features host fs::` passed 19 tests, with 2 ignored and 94 filtered out. `./tools/build arm` passed the smoke and CHOUPI firmware images. `./tools/env make -C choupi/os test` compiled but stalled at the first MPU ptrace-based emulator test; it was interrupted, so the full OS suite is not validated in this sandbox. No RISC-V build or flash was run.
- Result: Every injected interruption during a single-record replacement recovered either the complete previous value or complete replacement value; no torn value was read. This does not cover multi-record transactions, partial erase behavior, real power loss, or the planned 1,000 randomized cuts.
- Decision: Keep WP3 transaction meta-block and anti-tearing criteria open. Next design the persistent transaction journal and recovery path, then use the host cut injector to validate each commit/repair state.
- Commit: OS nested repository `9a2a25f`; root documentation commit recorded below.
- Artifacts/logs: `artifacts/logs/os-fs-powercut-host-test-20261016.log` (SHA-256 `0919809b7db96de712db8462d52815fd5227b7e8932ed36b314d4d059a4af3a3`); `artifacts/logs/build-arm-fs-unwind-20261016.log` (SHA-256 `44ea7a4731608e04d7ecf495bc15302ec8808db076e848c8e78264ce4d58d41f`).

## Filesystem interrupted-write validation commit metadata

- The progress, TODO, handoff, and validation artifacts for the single-record interrupted-write recovery step were committed as `9ca80ef`; its OS implementation and focused regression are in nested commit `9a2a25f`.

## 2026-10-16 — Defer RP2350 snapshot writes until transaction commit

- Step/experiment: `rp2350-jcsystem-transaction-flush`
- Change: Added an RP2350-visible persistent-transaction state query. Per-APDU `fs_sync()` now leaves overlay updates staged while JCSystem transactions are active. Commit flushes the new two-slot CRC snapshot before discarding rollback data; abort restores snapshots and flushes the restored view. A failed snapshot flush reports failure and attempts rollback. Extended the fake-flash overlay harness with byte-level cut injection and two-record recovery checks.
- Validation: `./tools/test-rp2350-fs-overlay` passed all checks, including 1,000 deterministic pseudo-random mutation cut-points across a two-record commit (`TEST:SUMMARY fail=0`). `./tools/test-persistent-transaction` passed 30 checks for begin/commit/abort, commit flush, abort flush, and failed-flush rollback. `./tools/build arm` passed both ARM images. A fresh PC VPCD JCAlgTest 1.8.2 applet pass completed in 63.380 s with `TEST:SUMMARY pass=11 fail=0`, including SELECT, GETVERSION, JCSystem, SHA-256, and support APDUs. No RISC-V build or flash was run.
- Result: Power-cut simulation restored the previous complete two-record snapshot at every tested byte cut; uncommitted writes remained staged and were discarded by simulated reset. This validates the RP2350 snapshot model in the host harness only, not actual flash behavior under power loss.
- Decision: The active RP2350 applet transaction no longer persists at APDU boundaries before Java commit. Keep real hardware power-cut validation open. The separate Rust OS filesystem still lacks its planned multi-record meta-block; Java transaction capacity and APDU exception-reason tests also remain.
- Commit: `e8d52b9`.
- Artifacts/logs: `artifacts/logs/rp2350-fs-overlay-transaction-cuts-20261016.log` (SHA-256 `62bae75c04a8ee8e52277f9a58db0b82ee60507627602340a03c2a0ddfb87353`); `artifacts/logs/persistent-transaction-flush-20261016.log` (SHA-256 `3f039b291755f205efca6be8e6189436fd075e66c1984cba0cd69989364f757b`); `artifacts/logs/build-arm-transaction-flush-20261016.log` (SHA-256 `3b8feb50dbaa4965e03a8758d84b3940e00315a46e0167c26e984516fc72b4da`); `artifacts/logs/host-vpcd-transaction-flush-20261016.log` (SHA-256 `1dcc8fb566b556a0a697c96c997f0642322afe3705e7705e6e05fe80f78f5b4a`, console summary).


## 2026-10-16 — Gate host filesystem write traces in release builds

- Step/experiment: `jcalgtest-static-sha-vpcd-log-gate`
- Change: Restricted per-record `PC_FS:WRITE` stderr logging to PC debug builds. The release VPCD emulator had been printing 665 write records during the official-client SHA-256 attempt, adding substantial log and host-I/O overhead.
- Validation: `./tools/build-pc` passed; `./tools/build arm` and `./tools/build riscv` both passed. `./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 90` passed 11/11; the 50-call, 256-byte SHA-256 static APDU returned `AA9000` in 15.578 s (about 312 ms per call in the PC emulator). No `PC_FS:WRITE` traces were emitted in the release test log.
- Result: Release emulator trace noise is removed while debug builds retain it. The host's 312 ms/call includes interpreter and VPCD overhead and is not Pico SHA throughput or a digest known-answer result. The archived official jCardSim static run reports SHA-256 `doFinal` at 0.05 ms/op with a negative minimum (-0.02 ms/op), showing its millisecond-scale timing is too coarse for this operation. A previous official client run through the VPCD PC/SC reader lost the card during SHA-256 measurement (`SCARD_W_REMOVED_CARD`); the current PC/SC service is unavailable (`SCARD_E_NO_SERVICE`), Java SmartcardIO sees no reader, USB/libusb access is unavailable in this VM, and no physical test ran.
- Decision: Keep official static performance measurements against a named CHOUPI reader open until PC/SC access is restored. Use the bounded VPCD APDU timing only as an emulator execution check; do not treat simulator sub-millisecond SHA numbers as performance results.
- Commit: `5356a01`.
- Artifacts/logs: `artifacts/logs/build-pc-jcalgtest-static-log-gate-20261016.log` (SHA-256 `bc7ac86694d011a971c79db6cbec97e7068901a8a9ac2e4ec7983f754c5acc91`); `artifacts/logs/build-arm-jcalgtest-static-log-gate-20261016.log` (SHA-256 `d1aeb972e951ddd93467ef5858c91b5344acf5747af00f71e5307f531c3263a3`); `artifacts/logs/build-riscv-jcalgtest-static-log-gate-20261016.log` (SHA-256 `5968948a154362fe184573f859255c90358cf974a63f4ba5d8b01bb9c5e33927`); VPCD log `artifacts/logs/jcalgtest-static-sha-vpcd-log-gate-20261016.log` (SHA-256 `33295e613ada7b1de5b026145bf60fd122d9c20b011f3b52f52a5bba5c61ba9b`) and console `artifacts/logs/jcalgtest-static-sha-vpcd-log-gate-20261016-console.log` (SHA-256 `1ca50e621071f63658eb3b4a66656bb16a8ed242853319ee5455d8337fb2f8c0`). Simulator CSV: `artifacts/results/jcalgtest-reader-selector-static-simulator-20261016/CHOUPI_JCARDSIM____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED_1792139077271__3B_FA_18_00_00_81_31_FE_45_4A_43_4F_50_33_31_56_32_33_32_98_(provided_by_Licel_jCardSim).csv` (SHA-256 `08dc50e8db26d30298f53743fbe883441174097b1a943eef562af5216fac7d86`).

## 2026-10-16 — Verify Java JCSystem transaction exceptions over VPCD

- Step/experiment: `jcsystem-transaction-java-apdu-probe`
- Change: Added a Java Card probe applet and host VPCD runner for transaction begin/depth/commit/abort, Java `TransactionException` reason codes, persistent field rollback, and subsequent-APDU persistence. Corrected exception handler range evaluation for caller frames after unwinding: the stored PC is the invoke opcode, while the handler's active range starts at the next bytecode boundary.
- Validation: Probe CAP conversion completed with zero errors and warnings. After removing temporary exception traces, `tools/test-jcsystem-transaction-applet` passed all 10 assertions. `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` completed successfully. A fresh broad PC VPCD JCAlgTest run passed 11/11, including the SHA-256 static APDU. No physical board flash or serial validation was done. The official static client invocation with `--reader-name 'Virtual PCD 00 00'` exited 4 because Java SmartcardIO reported no readers; no APDU was sent by that attempt.
- Result: The Java API's transaction reasons and persistent commit/abort behavior are now exercised through applet bytecode and APDUs. The focused run summary was `TEST:SUMMARY pass=10 fail=0 elapsed_s=8.946`. The broad post-fix VPCD run's 50-call SHA-256 static APDU took 19.679 s; this is PC interpreter/VPCD time, not hardware throughput or a digest known-answer test. Commit capacity queries remain zero and need implementation.
- Decision: Keep hardware validation outstanding. Next inspect storage limits and implement truthful maximum/unused commit-capacity values, then repeat official `ALG_PERFORMANCE_STATIC` once SmartcardIO exposes a named reader.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/build-jcsystem-transaction-probe-20261016.log` (SHA-256 `de16fcc3deb9dfac61a08937958aaefa7e2b74f8929c851e413a1829de8b3f5d`); `artifacts/logs/jcsystem-transaction-probe-clean-20261016.log` (SHA-256 `ce6592223494264c9007b2f66ebc920b19e7fa8f1ac99a7b2a41804b970b4e6a`); `artifacts/logs/jcsystem-transaction-probe-run-console-20261016.log` (SHA-256 `9b7dc79f6e110fd95125f04868b88ce39899134e753f2a3d5b244e961082e02a`); `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261016.log` (SHA-256 `49b3449724303e2c41c4e42aa6c532c96afe15d9b86cc7b7725b788f25b0e9e7`); `artifacts/logs/build-pc-jcsystem-transaction-clean-20261016.log` (SHA-256 `fc2251b7617bf2e58cdbc3dea0fb5531f2eef9f92a56ad209c697a534fbf71e9`); `artifacts/logs/build-arm-jcsystem-transaction-clean-20261016.log` (SHA-256 `f2b9a3192e11923dc69fb982782b626a81a2584a96283e30953270259312804c`); `artifacts/logs/build-riscv-jcsystem-transaction-clean-20261016.log` (SHA-256 `d938b3e505a17c05f449f73e2d9cb5e2e9352337584c4c73a7b99487e2af11ca`); `artifacts/logs/jcalgtest-static-named-vpcd-20261016.log` (SHA-256 `234be6ef20624509535b28813c2d921c388f1d300f1941f79be8a547b7ac477a`).

## JCSystem transaction APDU implementation commit metadata

- The transaction applet fixture, handler fix, 10-assertion VPCD probe, ARM/RISC-V/PC builds, and progress/TODO/handoff records above were committed as `cb6d21d`.

## 2026-10-16 — Recheck physical reader availability for static SHA validation

- Step/experiment: `physical-reader-access-recheck`
- Change: Rechecked USB, PC/SC, and libusb visibility before attempting the named-reader JCAlgTest static run or a board flash.
- Validation: `lsusb -d 2e8a:0009` and `lsusb -d 2e8a:000f` returned no device. `timeout 5s pcsc_scan -r` reported `SCardEstablishContext: Service not available`; its repeated invalid-handle output was captured only to a bounded file. `./tools/doctor` confirms build tools are present. No flash or hardware command was attempted.
- Result: This execution context has no visible physical Pico and no active PC/SC service. `picotool`/libusb initialization reports `-99`. The official named-reader static performance test cannot reach either a physical CHOUPI reader or Virtual PCD here. The archived simulator result for SHA-256 doFinal (256-byte input) is 0.05 ms/op average with -0.02 ms/op minimum, which is timer-resolution noise; no firmware change is justified from it. The available CHOUPI VPCD official log returned `AA 9000` for initial SHA-256 measurement APDUs but then lost the virtual card before writing a SHA-256 measurement row.
- Decision: Keep static performance and digest known-answer verification open until PC/SC and the intended reader are available. Use the direct target selection supported by `tools/run-jcalgtest`: `--reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID]'` for hardware or `--reader-name 'Virtual PCD 00 00'` for VPCD; use `--simulator` only for jCardSim reference data. Do not flash while guest USB enumeration is absent.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/pcsc-physical-access-check-20261016.log` (SHA-256 `b699fc4311fac1971545d8b9f21155d49e5a2f5b39f8b8a23ea735e3cf91763a`); `artifacts/results/jcalgtest-reader-selector-static-simulator-20261016/CHOUPI_JCARDSIM____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED_1792139077271__3B_FA_18_00_00_81_31_FE_45_4A_43_4F_50_33_31_56_32_33_32_98_(provided_by_Licel_jCardSim).csv` (SHA-256 `08dc50e8db26d30298f53743fbe883441174097b1a943eef562af5216fac7d86`); `artifacts/results/jcalgtest-static-vpcd-20261016/ALGTEST_log_1792133041312.log`.

## 2026-10-16 — Replace flooded PC/SC capture with bounded summary

- Step/experiment: `physical-reader-log-size-correction`
- Change: Replaced the 260,631,857-byte `pcsc_scan` stderr flood in the access-check artifact with a 478-byte diagnostic summary. The original bounded 5-second command produced 7,044,104 repetitive invalid-handle lines; no diagnostic information was removed beyond duplicate lines.
- Validation: Confirmed the compact artifact records the command, `SCardEstablishContext: Service not available`, the repeated invalid-handle condition, missing USB IDs, `-99` libusb result, and original byte/line count. New artifact SHA-256 is `82111d1919e6e1d523fae3c5ffe5bd7709d3532dd9e9051381f623e1f69ba7af`.
- Result: The working-tree artifact is bounded and readable. The preceding commit contains the original large blob in Git history; it has not been rewritten.
- Decision: Keep the concise artifact in the current tree and do not repeat `pcsc_scan` until the service can be started; its failure mode emits an unbounded error loop.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/pcsc-physical-access-check-20261016.log` (478 bytes; SHA-256 `82111d1919e6e1d523fae3c5ffe5bd7709d3532dd9e9051381f623e1f69ba7af`).

## Bounded physical-access artifact commit metadata

- The compacted diagnostic artifact and append-only correction record above were committed as `4337e94`.

## 2026-10-16 — Verify SHA-256 doFinal bytes and static operation

- Step/experiment: `sha256-dofinal-known-answer-and-static-vpcd`
- Change: Added a Java Card probe command for SHA-256(`abc`) and corrected `MessageDigest.OneShot.doFinal()` to retain one state-array reference, append the final input directly through `Sha256NativeBridge`, and finalize that same state. The previous virtual self-call to `update()` selected a different transient state array, so a call that reported success returned SHA-256 of the empty string.
- Validation: Probe CAP conversion completed. The host VPCD transaction+digest probe passed 11/11 assertions, including the expected digest `BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD`. The broader JCAlgTest VPCD regression passed 11/11; the exact 256-byte, 50-call static SHA-256 doFinal APDU returned `AA9000` in 16.853 s. `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` passed. No physical reader or flash was available.
- Result: Direct SHA-256 `doFinal(input,...)` and JCAlgTest's static doFinal path now produce the expected operation result; KAT verifies digest bytes for `abc`. The 16.853 s timing is PC interpreter/VPCD wall time, not Pico throughput. A separate Java `update("abc")` followed by `doFinal()` with zero new bytes still returns SHA-256 of the empty string; cross-call digest state remains broken and is explicitly open. Full official client static execution remains blocked because Java SmartcardIO sees no reader, and physical test remains blocked by absent guest USB/PCSC access.
- Decision: Continue by fixing cross-call SHA state/receiver behavior. Then repeat both known-answer cases and static configuration. Retry the official target-selected client only after PC/SC can see `Virtual PCD 00 00` or the Pico reader; keep hardware validation open.
- Commit: Java Card API nested commit `be4fade`; root test, result, and documentation commit recorded below.
- Artifacts/logs: `artifacts/logs/build-pc-sha256-dofinal-final-20261016.log` (SHA-256 `fc89228170759c16806c88b6324087f8fd8d486ae6bd5c83a6c2c464ce638e3f`); `artifacts/logs/build-arm-sha256-dofinal-final-20261016.log` (SHA-256 `d19173019e21e7255aef996426f2bac4507b438c2ca1e60f9911c1b71ab96137`); `artifacts/logs/build-riscv-sha256-dofinal-final-20261016.log` (SHA-256 `2555326761b3eef7d17cac21e97c2eb31d9bbddbf5d60e40bd54fe328b39624c`); `artifacts/logs/build-sha256-dofinal-final-applet-20261016.log` (SHA-256 `de16fcc3deb9dfac61a08937958aaefa7e2b74f8929c851e413a1829de8b3f5d`); `artifacts/logs/jcsystem-sha256-kat-final-console-20261016.log` (SHA-256 `f4b152fad1783cae59852b4a3836cf555f370380a858cfba4a455e64335a9f65`); `artifacts/logs/jcsystem-sha256-kat-final-vpcd-20261016.log` (SHA-256 `94128f8df5ac5aef170e8234555760a04599453d347199eaf30ab1976c72e27d`); `artifacts/logs/jcalgtest-sha256-dofinal-final-vpcd-20261016.log` (SHA-256 `dfc6ca30fb23dd7d3faec21a5c64cb6586269c3999acb98bc272cb968f4e413e`).

## SHA-256 doFinal validation commit metadata

- The known-answer probe, Java API `doFinal()` correction record, 50-call static VPCD regression, ARM/RISC-V/PC builds, and updated documentation above were committed as root `621e03c`; the API implementation is nested commit `be4fade`.

## 2026-10-16 — Validate method-local SHA-256 streaming use

- Step/experiment: `sha256-method-local-streaming-probe`
- Change: Added a Java Card probe command for a method-local `MessageDigest` that calls `update("abc")` and then empty `doFinal()`. Corrected the earlier status note: the failing probe retained the implementation's `OneShot` object in an applet field, despite its documented temporary-object lifetime. The factory currently returns this implementation, so retained digest support remains an API design gap; the failed field-retention test is not evidence of a general streaming failure. Removed temporary VM trace instrumentation.
- Validation: `./tools/build-pc` passed. `./tools/test-jcsystem-transaction-applet --timeout 90 --log artifacts/logs/jcsystem-sha256-stream-method-local-vpcd-20261016.log` passed 12/12; both SHA-256 paths returned `BA7816BF01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD`. `./tools/run-jcalgtest --reader-name 'Virtual PCD 00 00' -op ALG_PERFORMANCE_STATIC -fresh -outpath artifacts/results/jcalgtest-static-vpcd-readername-20261016` exited 4 because Java SmartcardIO found no reader. `pcsc_scan -r` reported `SCardEstablishContext: Service not available`. ARM/RISC-V builds and hardware flashing were intentionally skipped: this step retained no firmware/API source changes, and no PC/SC or physical reader was available.
- Result: SHA-256 known-answer and method-local streaming calls pass in host VPCD. The named-reader official client run and physical performance measurement remain unavailable. The prior 256-byte/50-call static VPCD result (`AA9000`, 16.853 s) is emulator wall time, not Pico throughput.
- Decision: Keep the passing method-local probe. Resolve the retainable `MessageDigest` implementation separately before claiming applet-field lifetime support; retry official `ALG_PERFORMANCE_STATIC` with `--reader-name` when Java can enumerate the target reader.
- Commit: recorded as root `12e1d71`; the SHA reference is added in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/build-pc-sha256-stream-probe-clean-20261016.log`; `artifacts/logs/jcsystem-sha256-stream-method-local-vpcd-20261016.log`; `artifacts/logs/jcsystem-sha256-stream-method-local-console-20261016.log`; `artifacts/logs/jcalgtest-static-vpcd-readername-20261016.log`.

## SHA-256 streaming probe commit metadata

- The method-local streaming probe, corrected status, continuity update, and validation record above were committed as root `12e1d71`. No Java Card API implementation change was retained in this step.

## 2026-10-16 — Recheck physical Pico availability

- Step/experiment: `physical-usb-access-recheck`
- Change: No source changes; checked guest USB, host diagnostics, PC/SC, and picotool access before a requested real-board test.
- Validation: `./tools/doctor` passed host prerequisites. `lsusb -d 2e8a:0009` and `lsusb -d 2e8a:000f` failed because libusb could not initialize (`-99`); `timeout 5s ./tools/env picotool info -a` failed with `Failed to initialise libUSB` (exit 250). `timeout 5s pcsc_scan -r` timed out with PC/SC service unavailable.
- Result: The current VM execution context cannot access the physical Pico. No builds or flash were attempted; no hardware result is claimed. Existing uncommitted worktree changes were preserved.
- Decision: Retry when guest USB passthrough is available; require both ARM and RISC-V builds to pass before flashing, then inspect the bounded cycle serial log.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: none; bounded diagnostic output is recorded here and in `.agent/HANDOFF.md`.

## Physical Pico availability recheck commit metadata

- The physical-access diagnostic and continuity note above were committed as `79c1568`.

## 2026-10-16 — Flash CHOUPI and check RP2350 runtime

- Step/experiment: `physical-choupi-cycle-after-replug`
- Change: No source change. Rechecked the replugged board, built both architectures, and flashed the ARM CHOUPI firmware through the repository cycle wrapper.
- Validation: `./tools/build arm` passed and emitted `build/arm/pico2_choupi.uf2`; `./tools/build riscv` passed and emitted `build/riscv/pico2_jcvm_smoke.uf2`. `./tools/cycle choupi` flashed successfully (`FLASH:PASS`) and captured 18 seconds of hardware serial output. RP2350 BOOTSEL was confirmed before the cycle. A five-second output-capped PC/SC scan found only `Virtual PCD 00 00/01`.
- Result: Physical runtime failed: repeated `CHOUPI:STATUS sw=6F00`, `TEST:FAIL choupi_runtime`, `TEST:SUMMARY pass=0 fail=1`; CCID state was `stage=2 rx_events=0 tasks=0`. The official physical `ALG_PERFORMANCE_STATIC` run was not possible because PC/SC did not expose the Pico reader. Hashes: ARM build `432c99587fbc2438704fea7d04af0cc2a6c3ef13bd3e35067ea333b467d58fc9`; RISC-V build `a5ccfe21a879f14e2f90b7e7211b06ab6d3abac7890d0a7f5c19fc1ebb43a5d2`; cycle `75a8e07f5e03db9aaf0d22192d097f2fb8bde72e6d33419b9735221abf5d50e1`.
- Decision: Treat this hardware check as failed. Diagnose startup `6F00` and physical PC/SC registration before repeating the JCAlgTest performance operation.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: `artifacts/logs/physical-retry-build-arm-20261016.log`; `artifacts/logs/physical-retry-build-riscv-20261016.log`; `artifacts/logs/cycle-choupi-20261016T154057Z.log`.

## Physical CHOUPI cycle commit metadata

- The physical cycle logs, failure record, and continuity update above were committed as `1585413`.

## 2026-10-16 — Fix Java Card array logical size and recover RP2350 startup

- Step/experiment: `jc-array-logical-size-hardware-runtime`
- Change: `JC_Array::size()` now converts transient and ordinary backing byte
  lengths to logical element counts using the element width. Persistent arrays
  continue to use the logical size in their record header. The mismatch caused
  applet field serialization to walk beyond an `Object[]` backing store.
- Validation: `./tools/build-pc` and `./tools/build arm` passed. The host VPCD
  JCAlgTest applet regression passed 11/11. The ARM image was flashed to the
  RP2350; the bounded serial capture repeatedly emitted
  `CHOUPI:STATUS sw=9000`, `TEST:PASS choupi_runtime`, and
  `TEST:SUMMARY pass=1 fail=0`. A bounded PC/SC check exposed only virtual
  readers when USB was accessible. A later recheck found guest libusb
  unavailable (`-99`) and PC/SC inactive. RISC-V build was intentionally
  skipped under the current ARM-only task scope.
- Result: The previous startup `6F00` was caused by interpreting backing bytes
  as array element count; hardware startup now passes. Host static SHA-256
  APDU time was 17.667 s on the PC interpreter and is not a Pico performance
  measurement. Physical JCAlgTest APDU/performance validation remains pending.
- Decision: Keep the fix. Restore USB passthrough and PC/SC reader registration,
  then run official JCAlgTest v1.8.2 `ALG_PERFORMANCE_STATIC` against the exact
  Pico reader name.
- Commit: `7817bd1` (implementation and logs).
- Artifacts/logs: `artifacts/logs/build-arm-jc-array-logical-size-20261016.log`,
  `artifacts/logs/build-arm-jc-array-startup-trace-cleanup-20261016.log`,
  `artifacts/logs/build-pc-jc-array-logical-size-20261016.log`,
  `artifacts/logs/host-vpcd-jc-array-logical-size-console-20261016.log`,
  `artifacts/logs/monitor-arm-jc-array-runtime-followup-20261016.log`, and
  `artifacts/logs/pcsc-arm-array-size-fix-20261016.log`.

## 2026-10-16 — Recheck physical PC/SC access after replug

- Step/experiment: `physical-reader-pcsc-acl-recheck`
- Change: No source change; checked the replugged Pico USB enumeration, USB
  node ACL, PC/SC daemon, and reader list using bounded diagnostics.
- Validation: Escalated `lsusb -d 2e8a:0009` found
  `Bus 001 Device 013: ID 2e8a:0009 Raspberry Pi Pico 2 CHOUPI CCID`;
  `pcscd.service` is active. `pcsc_scan -r` completed with only
  `Virtual PCD 00 00/01`. The USB node ACL grants `claude:rw-` but has no
  `pcscd:rw-` entry. Picotool reports no BOOTSEL device and says this RP2350
  currently has a USB serial connection.
- Result: Board is connected in normal CHOUPI firmware mode, but official
  Java SmartcardIO cannot reach it through PC/SC until the daemon receives USB
  access. Repository instructions prohibit autonomous sudo, so the already
  installed `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper was not run.
  No physical JCAlgTest operation was run.
- Decision: After the helper runs, confirm the exact Pico reader name and
  execute official JCAlgTest v1.8.2 `ALG_PERFORMANCE_STATIC`; inspect status
  words and SHA-256 measurement separately from the existing host timing.
- Commit: recorded in the following commit-metadata entry.
- Artifacts/logs: none; bounded outputs and ACL are recorded above and in
  `.agent/HANDOFF.md`.

## 2026-10-16 — Rebuild host emulator and locate VPCD startup timeout

- Step/experiment: `fresh-pc-vpcd-startup-diagnostic`
- Change: No lasting source change; rebuilt the PC emulator from the current
  worktree and traced the bounded JCSystem VPCD test's startup stages. Removed
  temporary diagnostic prints after the run.
- Validation: `./tools/build-pc` passed; the build emitted existing Rust and
  packed-member warnings. `./tools/test-jcsystem-transaction-applet --timeout
  120` failed with `timed out waiting for emulator VPCD connect`. Stage output
  shows the emulator enters the initial JCRE runtime but does not reach CAP
  installation or VPCD transport; no APDU was sent. A subsequent 20-second
  progress probe also failed to reach transport. No ARM flash or RISC-V build
  was attempted.
- Result: The previously archived 11/11 host VPCD pass used a PC binary built
  before later source edits; current-source host applet execution is not
  verified. The physical RP2350 startup pass remains a separate successful
  result.
- Decision: Keep the host startup timeout open. Trace the initial
  `Starter.run()` / `PlateformeGlobale.getInstance()` path with bounded
  instruction progress, then rerun the JCSystem and broad applet regressions.
- Commit: `e5e9efd` (build and failure logs).
- Artifacts/logs:
  `artifacts/logs/build-pc-after-jcsystem-diagnostics-cleanup-20261016.log`;
  `artifacts/logs/jcsystem-continuation-regression-fresh-20261016.log`.

## 2026-10-16 — Fail fast when host MPU ptrace setup is denied

- Step/experiment: `host-mpu-ptrace-fail-fast`
- Change: The host MPU emulator now checks `PTRACE_ATTACH`,
  `PTRACE_SETOPTIONS`, and `PTRACE_CONT`. On failure it kills/reaps the forked
  child and exits with the operation and OS error rather than blocking forever
  while waiting for a ptrace stop.
- Validation: `./tools/build-pc` passed (the existing Rust OS warnings remain).
  Under a bounded nested `strace` run, `PTRACE_ATTACH` returned `EPERM`; the
  emulator reported `Operation not permitted`, exited 1 in under one second,
  and `waitpid` confirmed the child was reaped. No ARM firmware change or
  RISC-V build was involved. The untraced host VPCD startup timeout remains
  separate and unresolved.
- Result: Host test/debug runs that cannot use ptrace fail clearly instead of
  consuming CPU in an indefinite wait. Normal host VPCD execution is not yet
  verified on the freshly built binary.
- Decision: Keep this host-only reliability guard. Continue diagnosing the
  direct JCRE runtime timeout and reattempt host applet regression after that
  path progresses; physical PC/SC requires the authorized helper.
- Commit: nested CHOUPI OS `f419dde`; root logs `d4ab895`.
- Artifacts/logs: `artifacts/logs/build-pc-ptrace-failfast-20261016.log`;
  `artifacts/logs/host-jcre-ptrace-after-fix-20261016.15050` through `.15053`.
## 2026-10-16 — Diagnose physical applet restore failure

- Step/experiment: `physical-ccid-restore-diagnostic-20261016`
- Change: No firmware change. Used direct, sequence-checked libusb CCID access
  to distinguish USB, PC/SC, and Java Card applet availability after replug.
- Validation: Host-access `lsusb` saw Pico 2 CHOUPI at bus 001 device 013;
  `picotool info -a` confirmed running mode. `pcsc_scan -r` listed only
  Virtual PCD 00 00/01 because pcscd lacks access to the `root:plugdev` USB
  node. Direct interface 2 claim/release and CCID power on succeeded with ATR
  `3B800181`. SELECT returned `6F00`; GETVERSION returned `6D00`.
- Result: Serial shows the JCAlgTest AID and CAP metadata resolve
  (`installed=1`, package 63, class `0094`), but instance restoration fails:
  `RESTORE_STORAGE status=1 length=0`, `RESTORE_READ_FAIL id=5
  tag=03003F94FF`, `SELECT restore=1`. The firmware startup smoke still reports
  `TEST:PASS choupi_runtime`; this is not an applet pass. Static performance
  results remain uncollected. Unsupported algorithms were not tested or
  changed.
- Decision: Fix/reinstall the broken physical applet persistent state through
  the supported card-manager flow, then rerun SELECT, GETVERSION, JCSystem,
  SHA-256, and official v1.8.2 `ALG_PERFORMANCE_STATIC`. No sudo, build, or
  flash was used because the card-manager path is not exposed through PC/SC.
- Commit: `74fcf20` (physical capture); documentation update follows.
- Artifacts/logs: `artifacts/logs/physical-ccid-restore-diagnostic-20261016.log`.

## 2026-10-16 — Replugged Pico applet probe

- Step/experiment: `physical-applet-delayed-init-recheck`
- Change: No firmware or CAP change. Rechecked the replugged board with direct
  CCID and captured serial during SELECT, GETVERSION, JCSystem, and SHA support
  APDUs.
- Validation: Host-access `lsusb` found `2e8a:0009` at bus 001 device 029;
  `picotool info -a` confirmed running mode. Direct CCID power-on returned ATR
  `3B800181`; SELECT returned `9000`. GETVERSION (`B0E1`), JCSystem info
  (`B073`), and SHA support (`B075`) each returned `F503`. The bounded serial
  trace shows persistent root restore succeeds, then delayed allocation throws
  before command dispatch. `pcsc_scan -r` lists only the two Virtual PCD
  readers. No build or flash was run.
- Result: Physical link is available, but the JCAlgTest applet is not yet
  operational. Startup `TEST:PASS choupi_runtime` does not validate applet
  behavior. SHA digest or static timing has not been measured.
- Decision: Trace the exception during delayed initialization and preserve
  unsupported-algorithm behavior; rerun hardware JCSystem, SHA known-answer,
  and static performance only after GETVERSION succeeds.
- Commit: `76f4a71`.
- Artifacts/logs: `artifacts/logs/monitor-arm-current-apdu-f503-20261016.log`.

## 2026-10-16 — Trace JC305 security static calls

- Step/experiment: `physical-jcalgtest-security-call-trace-20261016`
- Change: Added a bounded firmware diagnostic for imported `javacard.security`
  static calls, recording the JC305 class/method tokens, remapped class and
  method tokens, and resolved offsets. This is to identify the source of the
  delayed-init `CryptoException.NO_SUCH_ALGORITHM`.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed. The
  ARM image flashed with `FLASH:PASS image=build/arm/pico2_choupi.uf2`. A
  bounded pre-flash raw CCID probe again returned SELECT `9000` and `F503` for
  GETVERSION, JCSystem, and SHA support. After flash, the USB device
  disconnected/re-enumerated; a single bounded `lsusb`/picotool check found no
  accessible Pico, so no post-flash APDU or invocation trace was captured.
- Result: Source inspection narrows the previous trace to construction of
  `AlgKeyHarvest` after `AlgSupportTest` is stored; its constructor creates
  `TestSettings`, then calls `RandomData.getInstance(ALG_SECURE_RANDOM)` with
  algorithm value 2. The diagnostic build is now on the board; its exact
  resolved static call and post-flash applet behavior are unverified.
- Decision: Resume with a bounded USB visibility check. When the Pico
  reappears, capture finite serial output and repeat SELECT/GETVERSION/JCSystem
  first; inspect the security-call tokens before changing any remap table.
  Keep the real applet’s unsupported algorithms as expected behavior.
- Commit: pending; the workspace `.git` index is currently read-only.
- Artifacts/logs: `artifacts/logs/physical-jcalgtest-readonly-probe-20261016.log`,
  `artifacts/logs/physical-jcalgtest-security-call-trace-probe-20261016.log`,
  `artifacts/logs/monitor-arm-after-replug-apdu-20261016.log`,
  `artifacts/logs/build-arm-jc305-remap-fix-20261016.log`,
  `artifacts/logs/build-riscv-jc305-remap-fix-20261016.log`.

## 2026-10-16 — Correct JC305 security class remap

- Step/experiment: `jc305-security-class-remap-export-compare`
- Change: Restored the `javacard_security` remap entries to the class-name
  correspondence between the JC305U3 export and the generated CHOUPI security
  export. The worktree table had mapped JC305 `RandomData` token `0x0E` to
  token 30 (`MessageDigest$OneShot`) instead of token 28 (`RandomData`); other
  entries after the digest class were also mismatched. No algorithms were
  added or reclassified.
- Validation: The pinned exp-reader compared class names/tokens across both
  export files; all 34 JC305U3 classes have matching CHOUPI export tokens.
  `./tools/build arm` and `./tools/build riscv` both passed with no warnings.
  Physical access had failed its bounded post-flash visibility check, so this
  corrected mapping was not flashed or hardware-tested.
- Result: The map now matches the exports. The traced delayed-init exception
  is consistent with the incorrect RandomData class translation, but a
  successful GETVERSION/JCSystem run is still needed to confirm the fix.
- Decision: Keep the fix and the bounded static-call trace; when the board
  becomes visible again, rebuild both if source changed, flash ARM, and check
  GETVERSION then JCSystem before SHA/performance tests.
- Commit: `2ff11eb` (progress and evidence; the diagnostic source instrumentation
  remains uncommitted alongside pre-existing source changes).
- Artifacts/logs: `artifacts/logs/build-arm-jc305-remap-fix-20261016.log`,
  `artifacts/logs/build-riscv-jc305-remap-fix-20261016.log` and the export
  comparison output captured during this session.

## 2026-10-16 — Refresh active project tracker

- Step/experiment: `arm-only-active-stage-tracker`
- Change: Updated `TODO.md` to replace the stale PC/SC next step with the
  current physical `F503` delayed-init failure, the verified 34-class
  JC305U3 export-map comparison, and the required next ARM-only hardware
  checks. Updated the handoff to honor the user's ARM Cortex-M33-only scope.
- Validation: Reviewed the current `plan.md`, TODO, physical-access skill,
  latest direct CCID probe, finite serial trace, export comparison, and ARM
  build log. The latest ARM build passed; no RISC-V build was run for this
  tracker update. No target test was run because no firmware source changed.
- Result: Active acceptance work is accurately recorded. Physical applet
  process APDUs still fail before dispatch, and the corrected map has not yet
  been flashed or verified on hardware.
- Decision: Resume physical testing only after USB visibility returns. Then
  build ARM, flash, and require GETVERSION and JCSystem success before SHA-256
  and static-performance validation. Continue host-side work independently.
- Commit: `cd62665`.
- Artifacts/logs: `artifacts/logs/physical-jcalgtest-readonly-probe-20261016.log`,
  `artifacts/logs/monitor-arm-exception-trace-apdu-20261016.log`,
  `artifacts/logs/jc305-security-remap-export-compare-20261016.log`,
  `artifacts/logs/build-arm-jc305-remap-fix-20261016.log`.

## 2026-10-16 — Continue ARM-only host and physical validation

- Step/experiment: `physical-replug-host-vpcd-startup`
- Change: Corrected the persistent-field existence check to match `fs_exists()`
  semantics (`0` means the record exists). Refreshed TODO and handoff with the
  latest hardware and host-test state. A VPCD startup shortcut was explored and
  reverted after it bypassed installer context and failed during CAP install.
- Validation: `./tools/build arm` and `./tools/build-pc` passed. The fresh
  `./tools/test-jcsystem-transaction-applet --timeout 60` run failed before the
  VPCD socket connected; the preceding runtime log shows the slow Java JCRE
  startup loop (trace count 25,961). A 30-second attempt also timed out.
  `./tools/doctor` passed host prerequisites, while `lsusb -d 2e8a:0009` and
  `-000f` both failed to initialize libusb (`-99`); PC/SC reported an
  unavailable service and its scan was interrupted. Physical flash and APDU
  tests were intentionally skipped; RISC-V builds/tests were skipped per scope.
- Result: ARM and PC builds are good, but current host applet VPCD validation is
  blocked by slow JCRE startup and physical validation by VM USB/libusb access.
  The status correction did not make the VPCD endpoint connect within 60 s.
- Decision: Keep the normal JCRE path. Continue by diagnosing the startup loop
  without busy waiting; retry hardware only after guest USB enumeration works.
  Then run SELECT/GETVERSION/JCSystem, SHA-256, and named-reader static
  performance validation in that order.
- Commit: `cb6f8aa`.
- Artifacts/logs: `artifacts/logs/build-arm-fs-exists-status-fix-20261016.log`
  (SHA-256 `6926164d04d39df5cb0416d2dbdca20ce2fe2ae939def951572b226db75efcf4`),
  `artifacts/logs/build-pc-fs-exists-status-fix-20261016.log` (SHA-256
  `889020cbcb25d04f252149bc463f80ebfbc6d1e6c4c287db5c31752a1858f354`),
  `artifacts/logs/jcsystem-transaction-fs-exists-status-fix-20261016.log`
  (SHA-256 `18c03f329b3f87c3eb0c898c7a8744607a29210efa7d365dd97a3cbc98b90fe4`),
  `artifacts/logs/jcsystem-transaction-restored-startup-20261016.log`.

## Validation commit metadata

- Step: `physical-replug-host-vpcd-startup`
- Commit: `cb6f8aa` (`Fix persistent field existence check`).

## 2026-10-16 — Restore persistent JCAlgTest applet correctly

- Step/experiment: `fs-exists-semantics-and-static-sha-vpcd`
- Change: Corrected `JC_Instance::getFields()` to match the documented OS ABI:
  `fs_exists()` returns nonzero when a tag exists. The earlier `== 0` change
  caused a fresh JCAlgTest instance to read a missing field and fail SELECT.
- Validation: `./tools/build arm` and `./tools/build-pc` passed. The focused
  JCSystem/SHA probe passed 12/12 APDUs in 44.119 s. The fresh VPCD JCAlgTest
  flow passed 11/11 checks: ATR, SELECT, GETVERSION, JCSystem, SHA-256 support,
  doFinal, static SHA-256, and the selected support probes. Static workload
  used the official 256-byte / 50-call settings and returned `AA9000` in
  36.643 s. Physical checks still fail at libusb initialization (`-99`) and
  PC/SC service unavailable, so no hardware flash or test was performed.
- Result: Applet install/restore, JCSystem calls, and the static SHA-256
  workload complete in the PC VPCD emulator. The static operation is slow
  (36.643 s in this host emulator); this is not a Pico measurement. Unsupported
  RSA-SHA1 returned reason `03` and remains an expected unsupported result.
- Decision: Use this corrected existence check; retain ARM-only scope. Retry
  physical testing only after guest USB enumeration and PC/SC work. The official
  Java client still needs a working PC/SC reader; current static validation
  used its exact applet configuration through the bounded VPCD harness.
- Commit: `d6794e9` (validation record and logs), `925a92f` (source fix).
- Artifacts/logs: `artifacts/logs/build-arm-fs-exists-corrected-20261016.log`,
  `artifacts/logs/build-pc-fs-exists-corrected-20261016.log`,
  `artifacts/logs/jcsystem-transaction-security-class-map-fix-20261016.log`,
  `artifacts/logs/vpcd-fs-exists-corrected-final-transcript-20261016.log`,
  `artifacts/logs/vpcd-fs-exists-corrected-final-20261016.log`.

## 2026-10-16 — Verify applet restore across emulator processes

- Step/experiment: `jcalgtest-two-process-persistence`
- Change: Reused one flash image across two separate PC emulator processes to
  verify restoration of the installed JCAlgTest applet after process restart.
- Validation: `./tools/test-pc-vpcd-applet --passes 2 --connect-timeout 60
  --apdu-timeout 60` completed `TEST:SUMMARY pass=22 fail=0`. Both processes
  passed ATR, SELECT, GETVERSION, JCSystem info, SHA-256 support and doFinal,
  static SHA-256, and selected support probes. Static SHA took `36.939 s` and
  `37.244 s`; both returned `AA9000`. This is host VPCD only; Pico power-cycle
  persistence remains unverified.
- Result: Persistent applet state restores across emulator processes. RSA-SHA1
  reason `03` remains an expected unsupported result.
- Decision: Continue with official-client/report coverage and remaining
  GlobalPlatform/API work; retry hardware when USB access is restored.
- Commit: `fe72ee0`.
- Artifacts/logs:
  `artifacts/logs/vpcd-persistence-corrected-transcript-20261016.log`,
  `artifacts/logs/vpcd-persistence-corrected-20261016.log`.

## 2026-10-16 — Restore wrapper-based VM regression tests

- Step/experiment: `host-regression-wrappers`
- Change: Converted `tools/test-jcarray` from direct `CXX` invocation to the
  repository `tools/env cmake` wrapper. Added a maintained host CMake target
  with the PC mbedTLS configuration and a focused filesystem stub. Corrected
  the short-APDU test's `fs_read_1b_at` declaration and supplied a failing stub
  for persistent-array reads not exercised by that test.
- Validation: 12 repository wrappers passed: `test-apdu-util` (17),
  `test-short-apdu` (27), `test-vpcd-protocol` (14), `test-vpcd-session` (14),
  `test-gp-delete` (11), `test-gp-registry` (44), `test-install-status` (10,
  summary `fail=0`), `test-persistent-transaction` (30, `fail=0`),
  `test-rp2350-fs-overlay` (27, `fail=0`), `test-jcarray` (15),
  `test-jcvmarray` (2), and `test-pc-rommask` (7). `git diff --check` passed
  for the changed harness files. No implementation source changed in this
  step; ARM was not rebuilt.
- Result: All selected host regression checks pass, and the JCArray test now
  complies with the repository compiler-wrapper rule.
- Decision: Continue with the remaining API/GP integration items and retry the
  official client once PC/SC is usable. Physical validation remains open.
- Commit: `a322223`.
- Artifacts/logs: `artifacts/logs/test-apdu-util-continuation-20261016.log`,
  `artifacts/logs/test-short-apdu-fixed-20261016.log`,
  `artifacts/logs/test-vpcd-protocol-continuation-20261016.log`,
  `artifacts/logs/test-vpcd-session-continuation-20261016.log`,
  `artifacts/logs/test-gp-delete-continuation-20261016.log`,
  `artifacts/logs/test-gp-registry-continuation-20261016.log`,
  `artifacts/logs/test-install-status-continuation-20261016.log`,
  `artifacts/logs/test-persistent-transaction-continuation-20261016.log`,
  `artifacts/logs/test-rp2350-fs-overlay-continuation-20261016.log`,
  `artifacts/logs/test-jcarray-wrapper-fix-20261016.log`,
  `artifacts/logs/test-jcvmarray-continuation-20261016.log`,
  `artifacts/logs/test-pc-rommask-continuation-20261016.log`.

## 2026-10-17 — Validate JCAlgTest SHA workload on physical Pico

- Step/experiment: `physical-jcalgtest-sha-static-arm`
- Change: Rebuilt and flashed the current RP2350 ARM image through repository wrappers while investigating the explicitly requested ARM hardware failure. No source code changed and no RISC-V build was run.
- Validation: `./tools/build arm` passed. Initial direct CCID probe before flashing got SELECT `9000` but GETVERSION `F503`; serial reported `VPCD:APPLET_PROCESS status=F503` at opcode `0x93`. `./tools/flash build/arm/pico2_choupi.uf2` passed after its bounded CDC-to-BOOTSEL fallback. Post-flash direct CCID probes passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 support, prepare/doFinal, and static SHA. Finite serial capture reported `TEST:PASS choupi_runtime` with no `FAULT`. `pcscd.service` was inactive and `pcsc_scan -r` listed only Virtual PCD readers, so the official Java client was not run.
- Result: Physical JCAlgTest static SHA configuration (256 input bytes, 50 `doFinal` calls) returned `AA9000` in 0.718038 s. Direct doFinal returned `AA9000`; GETVERSION returned `312E382E325F6A63333035 9000`; JCSystem info returned `9000`. Serial repeatedly reports `WATCHDOG_RECOVERY cause=other` with a retained marker; this diagnostic state remains unexplained and needs inspection.
- Decision: The freshly flashed ARM image passes the physical applet and SHA static APDU path. Keep official PC/SC client execution outstanding until the service exposes the Pico reader; do not treat raw CCID as the official client run.
- Commit: `06e1438`
- Artifacts/logs: `artifacts/logs/jcalgtest-physical-resume-20261016.log` (SHA-256 `d40efe0e3f4baaf829b30376958bdab5606c1f61ef8b280a3dc389bd9cd8da13`); pre-flash failure trace `artifacts/logs/monitor-arm-current-apdu-f503-20261016.log`; `artifacts/logs/monitor-arm-physical-jcalgtest-20261017.log` (SHA-256 `aef7ff988e34e5631d94743425df311f82d773df7d5c37c3dd6f196ed22fd8ff`); UF2 SHA-256 `11254d99c6439f5313086c6164de9fc7f1dbe81a9fb56654ae29f6c2ca96f747`

## 2026-10-17 — Consume watchdog recovery markers once

- Step/experiment: `arm-watchdog-marker-consumption`
- Change: `main_arm()` now consumes the retained RP2350 scratch marker after copying it into RAM and clears the pending diagnostic flag after its first report. This prevents a stale trace from repeating every second and appearing to indicate repeated recoveries.
- Validation: `./tools/build arm` passed. `./tools/flash build/arm/pico2_choupi.uf2` passed through the bounded CDC-to-BOOTSEL fallback. `./tools/monitor --seconds 8` recorded exactly one `WATCHDOG_RECOVERY cause=other`, eight `TEST:PASS choupi_runtime` reports, and no `TEST:FAIL` or `FAULT`. Direct physical CCID checks passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 support, and JCAlgTest static SHA after the flash.
- Result: The one-time diagnostic behavior is confirmed on the Pico. Physical static SHA returned `AA9000` in 0.725005 s. The marker reported `cause=other`; this did not indicate a watchdog-caused reset. The official Java client remains unrun because `pcscd.service` is inactive and the physical reader is not exposed through PC/SC.
- Decision: Keep the marker-consumption fix. Continue with the remaining JCSystem capacity accounting and rerun the official named-reader client when PC/SC access is restored. No RISC-V build/test.
- Commit: `0f5091a`
- Artifacts/logs: `artifacts/logs/build-arm-watchdog-marker-20261017.log` (SHA-256 `e9f46e2033d4fea9fbb37b7ac4bb651dac5d9426e18cca8bb8b643fa13d0bc03`); `artifacts/logs/flash-arm-watchdog-marker-20261017.log` (SHA-256 `805ffb3c7c8aaecd78677c938284a57381fe6dbe2b1c08041e810c4f58d36fb1`); `artifacts/logs/monitor-arm-watchdog-marker-20261017.log` (SHA-256 `bca3af91d35e403fba9552edb1d9085cec066c9466457cc83dd3ac3fee157d47`); `artifacts/logs/jcalgtest-physical-watchdog-fix-20261017.log` (SHA-256 `16532eb6e72dbe3c705c8908aa02a919c22196d7c6c2f07c57233dc94a6b239c`)

## 2026-10-17 — Trace JCSystem memory accounting boundary

- Step/experiment: `jcsystem-memory-accounting-link-boundary`
- Change: Investigated returning free bytes from the Rust OS linked-list allocator to `JCSystem`. The experiment added a tiny exported free-byte getter, but the active RP2350 link does not include `choupi-os`; its symbol was undefined. Reverted only the experiment's C++ and nested allocator changes, preserving all prior dirty work.
- Validation: The first `./tools/build arm` failed at the final link with `undefined reference to jcvm_current_heap_free_bytes` from `jni.cpp`. After reverting the attempted changes, `./tools/build arm` passed. No flash or hardware test was performed for this experiment.
- Result: Root `CMakeLists.txt` confirms that platform startup and Rust OS integration are future work for this target. The Rust allocator cannot provide the active Pico VM's available-memory figure. `getAvailableMemory` and commit-capacity queries remain zero until accounting is exposed by the allocator/filesystem actually linked into the Pico image.
- Decision: Keep the conservative zero return; next inspect Pico's linked allocator and persistent-store accounting separately. No RISC-V build/test.
- Commit: `52d12fa`
- Artifacts/logs: `artifacts/logs/build-arm-jcsystem-memory-20261017.log` (expected link failure, SHA-256 `041726320551474891e78d708df5f0739ce6c82f1cafeea4cca53ddc9b06cf4d`); `artifacts/logs/build-arm-jcsystem-memory-reverted-20261017.log` (pass, SHA-256 `4b880a88cc4a4fcfac6885e30fbb63c3abc221460a743a28bb889b7597073cd2`)

## 2026-10-17 — Reject invalid transient-memory estimate

- Step/experiment: `physical-jcsystem-memory-accounting-probe`
- Change: Tested Newlib `mallinfo()` and `sbrk(0)` as transient-memory sources, then removed the experiment and restored the existing conservative-zero implementation when they returned impossible values. No lasting runtime behavior change.
- Validation: `./tools/build arm` and `./tools/build riscv` passed without compiler warnings. The diagnostic ARM image flashed successfully; its serial capture showed eight `TEST:PASS choupi_runtime` samples and no `TEST:FAIL` or `FAULT`. Raw JCSystem probes on the physical Pico returned extended transient values around `0x0220F0E2` (about 35 MB); a temporary direct `sbrk(0)` probe yielded the same out-of-SRAM range. The restored zero-reporting image also flashed, but its final JCSystem/SHA APDU check was blocked: `lsusb` showed `2e8a:0009`, the device node was `root:plugdev 0660`, this process lacks `plugdev`, and `pcsc_scan -r` listed only Virtual PCD readers. The 5-second picotool check confirmed the board was not in BOOTSEL. SHA-256 static workload was not rerun on the final image; the previous physical pass remains 0.725005 s (`artifacts/logs/jcalgtest-physical-watchdog-fix-20261017.log`).
- Result: The attempted Newlib estimates are invalid for this VM runtime and were reverted. Returning zero remains safer until the active allocator exposes trustworthy free-byte accounting. The restored image is flashed, but final physical APDU validation remains outstanding because the current process cannot open the re-enumerated USB node.
- Decision: Keep `JCSystem.getAvailableMemory` conservative at zero; after the user refreshes the configured PC/SC ACL with `sudo -n /usr/local/sbin/pico2-pcsc-reader`, rerun finite serial capture, JCSystem info, and the bounded JCAlgTest SHA-256 static workload. Do not invoke sudo from the autonomous cycle.
- Commit: `27456b6` (metadata confirmed in follow-up commit)
- Artifacts/logs: `artifacts/logs/build-arm-jcsystem-memory-zero-20261017.log`; `artifacts/logs/build-riscv-jcsystem-memory-zero-20261017.log`; `artifacts/logs/flash-arm-jcsystem-memory-zero-20261017.log`; `artifacts/logs/jcalgtest-physical-jcsystem-memory-zero-20261017.log`; `artifacts/logs/jcalgtest-physical-break-probe-20261017.log`; `artifacts/logs/physical-access-blocked-20261017.log`; earlier SHA pass `artifacts/logs/jcalgtest-physical-watchdog-fix-20261017.log`.

## 2026-10-17 — Add issue knowledge base and update physical-access findings

- Step/experiment: `issue-knowledge-base-pcsc-discovery`
- Change: Added `.agent/QKB.md` per the updated `AGENTS.md`, documenting confirmed USB detach/reattach recovery, the specifically permitted PC/SC ACL helper, the runaway `pcsc_scan` failure, the open repeated SHA static failure, and current PC/SC discovery state. Updated `.agent/HANDOFF.md` with this session's latest checks.
- Validation: Inspected the knowledge-base and handoff text. The authorized helper returned `PCSC:ACCESS_GRANTED node=/dev/bus/usb/001/057 service=pcscd.service`; escalated `lsusb` saw `2e8a:0009`; `systemctl is-active pcscd.service` reported `active`. `./tools/probe-jc-algtest` and the exact Java SmartcardIO name resolver both found no readers and sent no APDUs. The USB ACL includes `pcscd` and `claude`. PC/SC journal query returned no entries. No build, flash, or firmware test was run.
- Result: Guest USB enumeration and device-node permissions work, but active pcscd does not expose a reader to Java SmartcardIO. The static hardware test cannot safely start until reader discovery succeeds. The prior runaway `pcsc_scan` result makes it unsuitable for further diagnosis.
- Decision: Keep the issue open in `.agent/QKB.md`; diagnose bounded PC/SC reader publication, then rerun the APDU probe and official named-reader static test.
- Commit: `c1cf45c` (confirmed in follow-up metadata commit)
- Artifacts/logs: none created; checks were bounded and their output is summarized above.

## 2026-10-17 — Trace PC/SC reader discovery after USB ACL recovery

- Step/experiment: `pcsc-reader-publication-diagnostics`
- Change: Extended `.agent/QKB.md` and `.agent/HANDOFF.md` with CCID descriptor, libccid mapping, and pcscd socket-activation findings.
- Validation: Read-only `lsusb -v -d 2e8a:0009` confirmed interface 2 is class `Chip/SmartCard`, with CCID bulk endpoints `03` and `83`. Parsed `/etc/libccid_Info.plist`; VID/PID `0x2E8A`/`0x0009` maps to `Pico 2 CHOUPI CCID`. `systemctl status` showed pcscd had exited successfully via auto-exit after about one minute while `pcscd.socket` remained listening. Java reader enumeration had returned an empty list while querying. No service restart, scanner, build, flash, or APDU was run during this diagnostic step.
- Result: The USB descriptor and installed driver mapping are present. The remaining issue is why pcscd fails to publish the reader to active clients; idle auto-exit is expected socket-activation behavior and is not itself evidence of a failure.
- Decision: Keep reader discovery open. Continue with bounded diagnostics; consider VM USB detach/reattach because it previously restored guest visibility. Run applet tests only after SmartcardIO lists the physical reader.
- Commit: `3b7cad9` (confirmed in follow-up metadata commit)
- Artifacts/logs: none; descriptor, driver mapping, daemon state, and reader enumeration were inspected directly.

## 2026-10-17 — Reproduce SHA static workload across physical APDUs

- Step/experiment: `physical-sha-static-repeat-reproducer`
- Change: Extended `tools/JCAlgTestApduProbe.java` to verify ten consecutive single-call SHA-256 doFinal APDUs, then run JCAlgTest's exact 256-byte/50-call static prepare and measurement APDUs, followed by another single-call SHA APDU. Each result is checked for `AA9000`. Updated `.agent/QKB.md` and `.agent/HANDOFF.md` with the host/sandbox context split and newest physical findings.
- Validation: In the host-access execution context, the official `./tools/run-jcalgtest --reader-name 'Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F) 00 00' -op ALG_PERFORMANCE_STATIC -fresh` selected reader index 3, passed SELECT/GETVERSION/JCSystem, and produced five baseline calls averaging `529.0`. Its first 256-byte/50-call SHA-256 measurement returned `AA9000` in `761 ms`; the next identical measurement failed with `SCARD_E_READER_UNAVAILABLE` (client exit 1). After a full CDC-to-BOOTSEL/application reboot without flashing, the enhanced probe passed all ten single-call APDUs, one 50-call APDU, and one following SHA APDU (`AA9000` each). On a later reboot the PC/SC list intermittently showed only VPCD; direct CCID endpoint 0x03 timed out (`libusb -7`) before APDU. An eight-second serial capture stayed healthy with `TEST:PASS choupi_runtime`, no `TEST:FAIL`/`FAULT`, and only `GetSlotStatus` traffic. `./tools/doctor` passed host prerequisites. No firmware build or flash was performed.
- Result: A lone 50-call APDU followed by a single SHA call is healthy on the Pico. The failure in the official run occurs only after its preceding baseline and additional measured operation sequence, so the exact firmware state transition remains unresolved. A full MCU reset restores isolated SHA support. The later reader loss is intermittent and distinct from the sandbox's default lack of host USB/PCSC visibility; hardware commands must use the host-access context. Unsupported algorithms reported `NO_SUCH_ALGORITHM` as expected.
- Decision: Keep SHA and reader-recovery issues open in `.agent/QKB.md`. Next compare the official client's exact baseline/repeat sequence with the focused probe once physical PC/SC discovery returns; avoid `pcsc_scan` due the prior runaway process. No RISC-V build/test and no flash.
- Commit: `11f8379` (confirmed in follow-up metadata commit)
- Artifacts/logs: `artifacts/logs/jcalgtest-static-physical-hostctx-20261017.log` (SHA-256 `f423bcbca20ebaf211c02fc393171938afaef65b632f00ccff44d9e1cf738b90`); `artifacts/results/jcalgtest-static-physical-hostctx-20261017/ALGTEST_log_1792220519358.log` (SHA-256 `1994479186a06c2cc3b247f15c3f2eb82af9bc9bfe4ff3d3f7c547013a5ab3f0`); matching CSV (SHA-256 `b418d8f6bfe111c59dc7bad4e36563ab6b7de0c55aa6bcc29e7d9de75e0882bf`); `artifacts/logs/monitor-jcalgtest-static-hostctx-20261017.log` (SHA-256 `7fd345669b97c9ac62745f8d60e4ed5a8d4d8637eade79431cc2cd5bd2f69357`); `artifacts/logs/jcalgtest-sha-50call-followup-physical-20261017.log` (SHA-256 `afd4b7dab4483670cc5e44d39ac431047b6f15487e733c835f5296b71a2b183d`); `artifacts/logs/jcalgtest-sha-repeated-50-apdus-physical-20261017.log`; `artifacts/logs/jcalgtest-sha-rawccid-repeat-physical-20261017.log`; `artifacts/logs/monitor-jcalgtest-ccid-timeout-20261017.log`.

## 2026-10-17 — Reproduce static SHA state overflow on Pico

- Step/experiment: `physical-jcalgtest-static-state-overflow`
- Change: Extended the bounded JCAlgTest APDU probe to reproduce the official SHA-256 static order: prepare 256-byte/50-repeat settings, five zero-repeat baseline APDUs, and two measured APDUs. Added the reproduced failure mechanism and general reusable-state lesson to `.agent/QKB.md`; updated the handoff.
- Validation: `timeout --signal=TERM --kill-after=5s 60s ./tools/probe-jc-algtest` compiled the probe and ran against the named physical Pico. All five baseline APDUs returned `AA9000`; the first measured APDU returned `F105`, so the probe correctly exited nonzero. Concurrent `./tools/monitor --seconds 20` captured the applet exception and continuing healthy runtime/CCID status. An earlier replay passed measurement one then failed the next transmit with `SCARD_E_READER_UNAVAILABLE`. No firmware build or flash was performed; only the Java probe changed.
- Result: The reproduced `F105` is `CryptoException.ILLEGAL_USE` from SHA state append overflow: retained state plus the 256-byte input exceeds the 2,046-byte native input buffer. This confirms stale/over-limit digest state as a direct failure mechanism. Which cleanup/reset path leaves it stale, and whether it explains the separate PC/SC reader error, remain unresolved.
- Decision: Keep `ALG_PERFORMANCE_STATIC` open. Trace digest state cleanup on success and failure before treating the issue as fixed; apply the same multi-APDU/state-lifecycle checks to future stateful algorithms.
- Commit: `274aaea3af60a7e7ae979c2c4d899f91e032e9aa`
- Artifacts/logs: `artifacts/logs/jcalgtest-static-state-repro-20261017.log` (SHA-256 `dd713bfab8772a15d04661b9ebd244c8972cd3a2085d0b4f7d11ff08a0043ede`); `artifacts/logs/monitor-alg-static-state-repro-20261017.log` (SHA-256 `0c11622d7844e92db567de647d8b686321c2af87af92baa83e2041d8dedc6fa1`)

## 2026-10-17 — Check digest reprepare recovery

- Step/experiment: `physical-jcalgtest-digest-reprepare-recovery`
- Change: Extended the focused probe so a reproduced `F105` triggers one digest reprepare and compares a static measurement immediately afterward. Updated QKB and handoff with the recovery result and refined next-step diagnosis.
- Validation: The physical probe again returned `F105` after five successful zero-repeat baselines. Reprepare returned `AA9000`; the next 50-call APDU still returned `F105`. Concurrent bounded serial output confirmed allocation of a new 2,048-byte transient array, reference store, second `F105`, and continued `TEST:PASS choupi_runtime`/CCID processing. The probe exited nonzero as expected. No firmware build or flash was performed.
- Result: The current error is repeatable after a fresh digest prepare, so retaining only the old digest instance does not explain it. The VM's digest-array field access/transient array contents remain suspects; native length was not directly instrumented, so the exact internal state is not yet proven.
- Decision: Continue tracing object/array field identity and the SHA bridge's observed length before changing behavior. Keep the static SHA issue open; the independent single-call SHA tests passed previously.
- Commit: `aebbcff5b81573e417a9bf5384c68714e9cc831c`
- Artifacts/logs: `artifacts/logs/jcalgtest-static-reset-diagnostic-20261017.log` (SHA-256 `462ac435d379a4615aa7b9ab719c9f9cdddc9aa78484977b750db0a205bd54dd`); `artifacts/logs/monitor-alg-static-reset-diagnostic-20261017.log` (SHA-256 `418169398bd3ccff8a300edb3d92021cb1614992d864142e2c33def19208d72d`)

## 2026-10-17 — Fix repeated static SHA heap exhaustion

- Step/experiment: `physical-jcalgtest-static-sha-heap-fix`
- Change: Persistent `GETFIELD_A` now reads the restored reference from the instance field cache. This preserves identity for transient arrays held by persistent objects and stops allocating a replacement array wrapper on each read. Added the confirmed cause and reusable-object guidance to `.agent/QKB.md`; updated `.agent/HANDOFF.md`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed; the ARM firmware flashed and finite serial capture showed repeated `TEST:PASS choupi_runtime` with no `FAULT`. The focused physical JCAlgTest probe passed five baselines, two consecutive 50-call static SHA APDUs, ten single-call SHA APDUs, another static SHA APDU, and one following single-call SHA (all `AA9000`). Official JCAlgTest v1.8.2 `ALG_PERFORMANCE_STATIC` exited 0.
- Result: SHA-256 `doFinal()` averaged 2.01 ms/op over 250 invocations; update averaged 2.50 ms/op and reset 3.13 ms/op. Native trace before the fix recorded `current=0` on all 76 input appends and then `malloc 2060 failed to allocate memory` during the second static APDU, confirming heap exhaustion rather than stale digest length. Official run had 34 expected `NO_SUCH_ALGORITHM` rows and 1,866 separate `CANT_BE_MEASURED` rows, many returning `6F00`.
- Decision: Treat the repeated-state SHA failure as fixed on the tested physical board. Preserve the reference-identity/multi-APDU checks for future algorithms; investigate non-SHA `6F00` operations separately without implementing expected unsupported algorithms.
- Commit: `4debbf4e441c5687fb7b94c4e34b56b089cb70f6`
- Artifacts/logs: ARM build SHA-256 `6cc788ae0fc6ddff05a1d7ec37d99c7a8b95bea608c8933aed85388a883816a4`; RISC-V build `881208e73a2928a758eaea3326cdbbbb35e542c634fa9fff05f811ef6fe134b0`; flash `351869e7b877f1161b50e716cc7d9651f8eaba632082071a92940814c046bb90`; diagnostic serial trace `901c029c8f7430f5ab50a8b695c13b4157009746dd87b9c1f1cbbf600f92c2f7`; focused physical probe `9fb9185ceb587a261cf3755e053dd1a81ce59df51f18c3f115f6b0cf881a21f8`; official client `295243b8897c6eb59e1dd06a8dc8fc499bc602995088a38a07f9af263911765e`; post-fix serial `4584bb17cc0217f5639b9350b3b7bbb8fe584da437410a30bc0d81ff4949a4ba`. Full paths are listed in `.agent/QKB.md`.

## 2026-10-17 — Classify unsupported-algorithm static `6F00`

- Step/experiment: `jcalgtest-static-unsupported-prepare-triage`
- Change: Recorded the source of the first non-SHA `6F00` in `.agent/QKB.md` and `.agent/HANDOFF.md`: JCAlgTest v1.8.2 prepared `RandomData.ALG_TRNG`, received `03 9000` (`NO_SUCH_ALGORITHM`), then still sent the measurement APDU which returned `6F00`. The supported `ALG_SECURE_RANDOM` preparation and 50-call measurement returned `AA9000`.
- Validation: Read-only inspection of the captured official physical client and raw result logs confirmed the APDU order, specifications, statuses, and method labels. No build, flash, hardware command, or code test was run; this was log triage only.
- Result: The first residual `6F00` follows an explicitly unsupported algorithm and is not evidence of failure in a supported operation. This does not classify all 1,866 `CANT_BE_MEASURED` rows; their prepare statuses need individual inspection.
- Decision: Do not implement TRNG to eliminate this row. For remaining triage, investigate only a measurement whose prepare APDU returned `AA9000`.
- Commit: `ea7cc5ca38153758d01f91ba90dc216d386b1ebf`
- Artifacts/logs: `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log` (SHA-256 `295243b8897c6eb59e1dd06a8dc8fc499bc602995088a38a07f9af263911765e`); `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`.

## 2026-10-17 — Correct first static `6F00` classification

- Step/experiment: `jcalgtest-static-setseed-first-failure-correction`
- Change: Corrected the static-run triage in `.agent/QKB.md` and `.agent/HANDOFF.md`. The first failed operation is supported `RandomData.ALG_SECURE_RANDOM.setSeed()`: prepare and five zero-repeat baselines returned `AA9000`, but its first 50-repeat measurement returned `6F00`. Later TRNG statuses occur after that failure and cannot be classified independently.
- Validation: Re-parsed the raw official-client result in chronological APDU order and inspected the SDK `RandomData.setSeed()` implementation. The current container cannot access USB (`lsusb` and picotool report libusb initialization errors); the authorized helper is blocked by `no new privileges`. No new hardware test or build was run.
- Result: The first non-SHA `6F00` is a supported-path failure. The cause of repeated `setSeed()` failure is not yet established. Prior note attributing the earliest failure to unsupported TRNG is superseded.
- Decision: Reproduce only the secure-random prepare, zero-repeat baseline, and 50-repeat `setSeed` APDU in host-access context with bounded serial capture before changing code. Preserve later unsupported-algorithm interpretation until isolated evidence exists.
- Commit: `0b56df098fd7a8f65d2bdd67f0dad19a4873267f`
- Artifacts/logs: `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`; client transcript `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log` (SHA-256 `295243b8897c6eb59e1dd06a8dc8fc499bc602995088a38a07f9af263911765e`).

## 2026-10-17 — Fix RandomData virtual token mismatch

- Step/experiment: `physical-jcalgtest-randomdata-token-order`
- Change: Added the JC305-to-embedded virtual method token map for `RandomData` and `RandomData.OneShot`. JC305 token 2 is void `setSeed`; the embedded API uses token 3, while embedded token 2 is short-returning `nextBytes`. The mismatch caused an unexpected short return to remain on the caller stack and overflow on the next loop iteration. Documented root cause and follow-up in `.agent/QKB.md` and `.agent/HANDOFF.md`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed clean ARM image with `./tools/flash build/arm/pico2_choupi.uf2`. Focused physical setSeed repeat-50 probe returned `AA9000`; clean finite serial output showed `TEST:PASS choupi_runtime` and no `FAULT`. Official `ALG_PERFORMANCE_STATIC` against the named physical reader exited 0; secure-random setSeed passed 250 invocations at `0.36 ms/op` average. Official totals were 40 expected `NO_SUCH_ALGORITHM` and 1,855 `CANT_BE_MEASURED`; the first later `6F00` was DES Cipher prepare (`B031`), not yet classified.
- Result: Confirmed/fixed a cross-version virtual token-order defect in a supported void method. Subsequent TRNG prepare now independently reports `03 9000` (`NO_SUCH_ALGORITHM`). A separate DES Cipher prepare failure remains for targeted investigation.
- Decision: Keep unsupported algorithms as expected. Next isolate the supported-looking DES Cipher prepare APDU and inspect its exception/method map before changing cipher behavior.
- Commit: `daed80981836259686bb31ef27f056b27946e2af`
- Artifacts/logs: ARM build SHA-256 `dc10263786a33058c187db7d9105a4f84f8585d4960af4d6d5b9c3d28376a8f1`; RISC-V build `1a49f6dfe1fda8cf7517d15d7eea430122e89e1fe03731b0c35a0f35ede88be2`; flash `351869e7b877f1161b50e716cc7d9651f8eaba632082071a92940814c046bb90`; focused probe `artifacts/logs/jcalgtest-secure-random-setseed-clean-50-20261017.log` (SHA-256 `2d73aab519f2050881bcb7d80ce82fbea3cf0937cdccf121654d27c17a090b69`); full official run `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log` (SHA-256 `266df057ddd8df658cd339ea1b61ae5916a6b2396ad9ba8cb592a71b5d9fda20`); finite serial `artifacts/logs/monitor-arm-randomdata-clean-20261017.log`; raw result and CSV under `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/`.

## 2026-10-17 — Isolate DES static prepare failure

- Step/experiment: `physical-jcalgtest-des-prepare-probe`
- Change: Added `--des-cipher-prepare` to the focused JCAlgTest APDU probe using the exact first failing DES `B031` payload. Recorded the preparation call chain and current hardware-access blocker in `.agent/QKB.md` and `.agent/HANDOFF.md`.
- Validation: `PICO2_APDU_TIMEOUT_SECONDS=5 ./tools/probe-jc-algtest --des-cipher-prepare` compiled the probe, then exited 1 before transmission because Java SmartcardIO could not find the Pico reader. The permitted ACL helper could not run because this container sets no-new-privileges. Physical validation was skipped.
- Result: The DES prepare failure is narrowed to key build/set, Cipher factory/init, or random input fill; its root cause remains unknown because the available serial capture ends before the DES APDU.
- Decision: Replay this single APDU with bounded serial capture in host-access context; classify the exception before changing behavior.
- Commit: `6b04f1d21512bb80dab6716bedf574f6395a668a`
- Artifacts/logs: Existing official APDU `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log`; existing finite serial capture `artifacts/logs/monitor-jcalgtest-static-randomdata-token-map-20261017.log`; no new hardware log.

## 2026-10-17 — Fix JC305 Cipher dispatch in physical static test

- Step/experiment: `physical-jcalgtest-static-cipher-token-order`
- Change: Corrected JC305-to-embedded virtual method token translation for `javacardx.crypto.Cipher` and aligned the related DESKey interface lookup with the receiver package and canonical interface identity. Documented the root cause and future token-table checks in `.agent/QKB.md`; updated `.agent/HANDOFF.md`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed ARM firmware to the Pico 2; bounded serial boot capture reported `TEST:PASS choupi_runtime` with no `TEST:FAIL` or `FAULT`. `./tools/probe-jc-algtest --des-cipher-prepare` returned `AA9000`. Official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed with exit 0; the previously failing DES CBC no-padding doFinal/init rows returned `AA9000`. The full client reported 68 `NO_SUCH_ALGORITHM` and 1,824 `CANT_BE_MEASURED`; first later `6F00` was a separate 3-key DES CBC ISO9797 M1 prepare. No attempt was made to implement unsupported algorithms.
- Result: Confirmed that a JC305/embedded virtual token-order mismatch dispatched void `Cipher.init()` to a byte-returning method and corrupted the caller stack. The exact first DES prepare now passes on hardware; later 3-key DES failure remains open.
- Decision: Preserve import/export method-order comparisons as a compatibility check. Triage the first remaining failure at its prepare APDU before changing algorithm support.
- Commit: `aca9c3e` (metadata recorded in a follow-up commit)
- Artifacts/logs: `artifacts/logs/jcalgtest-des-cipher-token-map-fix-20261017.log` (SHA-256 `6e0997e4d9de771fdb7d69d664986bbdfdd48512a98fe914beccfe773e6ee3f9`); `artifacts/logs/jcalgtest-static-des-cipher-token-map-fix-20261017.log` (SHA-256 `0bd56cada46732f5eb399b3a9c59808ddf7466de67fde87ed11fe6aee4795980`); raw result `artifacts/results/jcalgtest-static-des-cipher-token-map-fix-20261017/ALGTEST_log_1792236553761.log`; CSV and bounded serial capture are in the same result/log directories. Build and flash logs are under `artifacts/logs/`.

## 2026-10-17 — Remove temporary DES dispatch diagnostics

- Step/experiment: `physical-jcalgtest-des-diagnostic-cleanup`
- Change: Removed temporary virtual/interface dispatch `printf` traces used to identify JC305 token order. Kept the verified method translation and cross-package interface resolution.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed the final ARM image to the physical Pico. Bounded boot and APDU captures reported `TEST:PASS choupi_runtime`, no `TEST:FAIL` or `FAULT`; `./tools/probe-jc-algtest --des-cipher-prepare` returned `AA9000` after cleanup.
- Result: The fix passes on the final firmware without per-dispatch diagnostic output. The official full static suite was run successfully on the preceding diagnostic image with the same functional mappings; the remaining first 3-key DES ISO9797 M1 `6F00` remains open.
- Decision: Keep the production virtual/interface dispatch path free of this investigation's high-volume trace output; next isolate the later prepare failure only if continuing suite triage.
- Commit: `2743bfb` (metadata recorded in a follow-up commit)
- Artifacts/logs: `artifacts/logs/build-arm-des-cipher-cleanup-20261017.log`; `artifacts/logs/build-riscv-des-cipher-cleanup-20261017.log`; `artifacts/logs/flash-arm-des-cipher-cleanup-20261017.log`; `artifacts/logs/monitor-arm-des-cipher-cleanup-boot-20261017.log`; `artifacts/logs/monitor-arm-des-cipher-cleanup-apdu-20261017.log`; `artifacts/logs/jcalgtest-des-cipher-cleanup-20261017.log`.

## 2026-10-17 — Correct JSR saved-PC storage

- Step/experiment: `physical-jcalgtest-jsr-frame-pc-reuse`
- Change: Changed saved JSR PCs from references to value copies, recorded the
  fall-through address, reused a saved slot after `ret`, and report overflow
  beyond 256 active return slots as `StackOverflowException`.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed the
  final ARM image; bounded serial boot reported `TEST:PASS choupi_runtime`.
  Physical focused probe passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info,
  and DES prepare (`AA9000`). The official full static suite reached the first
  AES-128 CBC no-padding `Cipher_doFinal` measurement after five successful
  baselines (`654.6 ms` average), then failed: serial reported
  `malloc 44 failed to allocate memory` and `*** PANIC *** Out of memory`;
  JCAlgTest returned `SCARD_E_READER_UNAVAILABLE`.
- Result: JSR saved-PC reference aliasing and unbounded per-frame slot growth
  are corrected, but this is not the complete static-suite OOM cause. The
  final 44-byte allocation owner remains unknown. A fixed-stack CBC experiment
  was reverted because it did not resolve exhaustion.
- Decision: Instrument VM frame depth and heap object counts around the exact
  repeated AES APDU before another full static run. Keep unsupported
  `NO_SUCH_ALGORITHM` results classified as expected.
- Commit: `f689286`
- Artifacts/logs: `artifacts/logs/build-arm-jsr-pc-slot-reuse-final-20261017.log`;
  `artifacts/logs/build-riscv-jsr-pc-slot-reuse-final-20261017.log`;
  `artifacts/logs/flash-arm-jsr-pc-slot-reuse-final-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-final-20261017.log`;
  `artifacts/logs/jcalgtest-jsr-final-reader-probe-20261017.log`;
  `artifacts/logs/jcalgtest-static-jsr-pc-slot-reuse-final-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-static-final-20261017.log`.

## 2026-10-17 — Identify physical static-suite heap retention

- Step/experiment: `physical-jcalgtest-static-heap-retention`
- Change: Added temporary per-APDU VM array/instance and frame high-water
  counters to diagnose the remaining AES static-suite OOM, then removed that
  instrumentation after capturing evidence. Updated `.agent/QKB.md` and
  `.agent/HANDOFF.md` with the root cause and safe collector design constraint.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with
  counters; the diagnostic ARM image flashed, bounded boot passed, and the
  named Pico reader probe passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem
  info, and DES prepare. Official JCAlgTest 1.8.2 static testing passed five
  AES-128 CBC no-padding `Cipher_doFinal` baselines (average `566.4 ms`), then
  lost the reader on the first 50-call measurement. Cleanup ARM/RISC-V builds
  passed, the non-instrumented ARM image flashed, and bounded serial boot
  reported `TEST:PASS choupi_runtime` with no `TEST:FAIL`/`FAULT`.
- Result: Completed APDUs ended with frame count 0 and a maximum frame
  high-water of 11. Cipher measurements accumulated about two arrays per
  repeat; before the failing APDU the counters showed `arrays=594`,
  `instances=192`. `Heap` has no reclamation path and
  `JCSystem.requestObjectDeletion()` is a no-op, confirming uncollected Java
  heap objects as the remaining OOM cause. SHA digest `B041` calls kept array
  counts flat. Official suite remains failed; `NO_SUCH_ALGORITHM` remains
  expected.
- Decision: Do not clear offset-indexed heap entries or pool objects without
  tracing all live roots. Next implement a GC/root-lifetime design that keeps
  `jref_t` references valid, then rerun static JCAlgTest on hardware.
- Commit: `c59114e`
- Artifacts/logs: official transcript, per-APDU serial capture, post-failure
  watchdog/boot capture, reader probe, both architecture builds, flash and
  clean boot logs are committed under `artifacts/logs/*jsr-memory-counters*`
  (hashes and paths in `.agent/QKB.md`). The official raw client log and
  partial CSV are committed under
  `artifacts/results/jcalgtest-static-memory-counters-20261017/`.

## 2026-10-17 — Record static-suite evidence digests

- Step/experiment: `physical-jcalgtest-static-heap-retention-evidence`
- Change: Added SHA-256 digests for the official client transcript, serial
  trace, post-failure boot, and clean firmware boot to `.agent/QKB.md` so later
  sessions can verify the captured root-cause evidence.
- Validation: Verified each digest with `sha256sum`; no source behavior
  changed and no additional board test was needed.
- Result: Evidence now has stable content identifiers in the QKB.
- Decision: Use the archived client and serial logs as the baseline when
  designing and validating heap reclamation.
- Commit: `c59114e`
- Artifacts/logs: Hashes recorded in the `Physical ALG_PERFORMANCE_STATIC
  heap retention` section of `.agent/QKB.md`.

## 2026-10-17 — Refresh project tracker from physical static run

- Step/experiment: `update-todo-static-oom-status`
- Change: Replaced the stale “official client outstanding” status with the
  actual physical AES static failure and added memory accounting, supported
  mode comparison, golden tests, and AC1–AC5 as next work.
- Validation: Inspected the archived official client transcript and serial
  counters against the updated status. No firmware behavior changed.
- Result: TODO now distinguishes the failed optional static benchmark from the
  still-unverified supported-algorithms acceptance mode.
- Decision: Next implement measured JCSystem memory accounting for the Pico's
  linked allocator and persistent store; ARM M33 only.
- Commit: `d872f1d`
- Artifacts/logs: `artifacts/logs/jcalgtest-static-memory-counters-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-static-20261017.log`.

## 2026-10-17 — Measure JCSystem memory and guard physical USB tools

- Step/experiment: `physical-jcsystem-memory-and-access-context-guard`
- Change: Added Pico-backed transient and persistent `getAvailableMemory`
  providers; scalar results saturate at `0x7fff`, and the jc304 array form
  returns high/low words. Added a live-memory JCAlgTest probe and a
  `NoNewPrivs` preflight to flash, serial, and physical PC/SC wrappers. Updated
  the QKB, handoff, and TODO with the measured capacities and safe invocation
  procedure.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Shell
  syntax checks passed. Flashing ARM firmware through host access passed;
  eight-second monitor output reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. Physical JCAlgTest 1.8.2 probe passed, including
  repeated SHA-256 calls. Live JCSystem probe passed: persistent `47,777`
  bytes; transient reset/deselect `325,352` bytes. Five physical wrappers
  invoked in the restricted context exited with the host-access diagnostic in
  under one second. One pre-guard restricted flash/monitor attempt failed due
  unavailable libusb/serial; subsequent hardware operations used host access.
- Result: Both short and jc304 extended memory forms are active on Pico. The
  persistent and deselect scalar values agree with their saturated extended
  values. The applet's reset-RAM field includes its own benchmark-array sizes;
  it is not the raw scalar value. Commit-capacity methods remain zero while
  transactions are unsupported.
- Decision: Always invoke flash, monitor, and physical PC/SC commands through
  host access; wrappers now fail immediately in the restricted context. Next
  add focused golden tests for JCSystem memory and applet lifecycle behavior,
  then continue the remaining acceptance criteria. The static-suite heap
  retention issue remains open as documented in `.agent/QKB.md`.
- Commit: `a9a2091`
- Artifacts/logs: `artifacts/logs/build-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/build-riscv-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/flash-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/monitor-arm-jcsystem-memory-accounting-continue-20261017.log`,
  `artifacts/logs/jcalgtest-physical-jcsystem-memory-accounting-continue-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-jcsystem-live-memory-continue-20261017.log`.

## 2026-10-17 — Close restricted-context USB entry points

- Step/experiment: `usb-restricted-context-fail-fast-coverage`
- Change: Added a shared Python `NoNewPrivs` guard to direct-libUSB bridge and
  repair tools, and moved the `cycle` guard ahead of its builds. This closes
  entry points that bypassed the existing shell-wrapper checks and prevents
  two unnecessary builds before a restricted flash failure.
- Validation: Shell syntax and Python compilation checks passed. In the
  restricted context, cycle, bridge, repair, flash, monitor, APDU probe, live
  JCSystem probe, and named physical JCAlgTest exited with the host-access
  diagnostic in 0.75–1.52 seconds. No USB/PCSC attempt occurred; cycle started
  no builds.
- Result: All known USB/PCSC entry points fail immediately in this restricted
  context; normal USB work must be invoked via host-access execution.
- Decision: Keep host-access invocation as the default and treat these checks
  as protection from accidental restricted invocations. Continue focused
  JCSystem/lifecycle golden coverage next.
- Commit: `68c3aa3`
- Artifacts/logs: none; timings captured from bounded command results.
## 2026-10-17 — Add JCSystem memory overload golden coverage

- Step/experiment: `jcsystem-memory-overloads-vpcd-golden`
- Change: Extended the focused Java Card transaction probe to exercise scalar
  and 32-bit `getAvailableMemory` calls for persistent, reset, and deselect
  memory, plus maximum and unused commit capacity. Added expected desktop
  output and marked the focused memory golden coverage complete in TODO.
- Validation: `./tools/build-pc` passed and regenerated the ROM mask; Java Card
  CAP conversion passed. The first restricted VPCD run could not bind its
  loopback socket; rerunning the bounded test via host access passed
  `TEST:SUMMARY pass=13 fail=0 elapsed_s=25.154`.
- Result: Desktop scalar and extended memory calls agree at zero for all three
  types; commit capacity queries remain zero. Prior physical Pico probe
  measured persistent `47,777` bytes and transient reset/deselect `325,352`
  bytes and confirmed scalar saturation/extended consistency.
- Decision: Both overloads are now covered by a golden applet call path and a
  physical-value probe. Continue lifecycle callbacks and remaining JCSystem
  acceptance.
- Commit: `7734d9c`
- Artifacts/logs: `artifacts/logs/jcsystem-memory-overloads-vpcd-20261017.log`.

## 2026-10-17 — Return documented exception for unsupported OneShot digests

- Step/experiment: `initialized-digest-oneshot-unsupported-factory`
- Change: Changed `InitializedMessageDigest.OneShot.open(byte)` from a null
  TODO result to `CryptoException.NO_SUCH_ALGORITHM`; added a golden applet
  call and corrected the test harness to print PASS only after expected bytes
  match. Documented the official-suite failure and distinction from expected
  unsupported algorithms in `.agent/QKB.md` and `TODO.md`.
- Validation: `./tools/build arm` and `./tools/build-pc` passed. Rebuilt the
  PC ROM mask before the host-access VPCD test; it passed
  `TEST:SUMMARY pass=14 fail=0 elapsed_s=23.936`. ARM firmware flashed and an
  8-second physical serial capture reported `TEST:PASS choupi_runtime` with no
  `TEST:FAIL` or `FAULT`. The bounded physical JCAlgTest probe passed SELECT,
  GETVERSION `1.8.2_jc305`, JCSystem info, repeated SHA-256 including 50 calls,
  and expected unsupported RSA-SHA1. No RISC-V build/test was run.
- Result: The official physical `ALG_SUPPORT_EXTENDED` sweep passed the
  previous OneShot failure; its CSV records
  `InitializedMessageDigest.OneShot / ALG_SHA_256;no`. It then stopped after
  1,637 CSV rows with `SCARD_E_READER_UNAVAILABLE` during
  `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384`. After the Pico re-enumerated, the
  bounded physical probe passed again. The full sweep remains incomplete and
  is not reported as a pass. Unsupported algorithms remain expected.
- Decision: Preserve the factory exception fix. Diagnose the later physical
  reader disconnect separately; do not infer allocation failure from this
  run because no allocation error was captured. Keep physical calls in
  host-access context; the prior USB guard commit covers accidental restricted
  invocations.
- Commit: pending
- Artifacts/logs: `artifacts/logs/build-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/build-pc-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcsystem-oneshot-unsupported-vpcd-final-20261017.log`,
  `artifacts/logs/flash-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/monitor-arm-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcalgtest-physical-oneshot-unsupported-fix-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-oneshot-fix-20261017.log`,
  `artifacts/logs/monitor-arm-oneshot-unsupported-after-official-20261017.log`,
  `artifacts/logs/jcalgtest-physical-after-supported-20261017.log`.

## 2026-10-17 — Capture and recover supported-sweep PC/SC loss

- Step/experiment: `supported-sweep-ccid-pcsc-disconnect-reproduction`
- Change: Reproduced the named-reader disconnect while capturing serial
  concurrently; recorded the failure layer, controlled reflash recovery, and
  remaining uncertainty in `.agent/QKB.md`, `TODO.md`, and `.agent/HANDOFF.md`.
- Validation: Ran the official physical `ALG_SUPPORT_EXTENDED` mode again
  with a bounded 240-second monitor. It stopped at
  `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384` after 1,636 CSV data rows with
  `SCARD_E_READER_UNAVAILABLE`. Serial showed RX event 3,556 / transfer 2,909
  and normal runtime PASS, with no firmware fault, allocation failure, `6F00`,
  or reset. USB still enumerated as `2e8a:0009`, but PC/SC exposed only VPCD
  after ACL refresh. Reflashed the already-built ARM image; `FLASH:PASS`,
  bounded runtime monitor passed, PC/SC republished Pico, and physical probe
  passed including the SHA 50-call workload and expected unsupported result.
- Result: Reader publication and APDU service returned after firmware
  re-enumeration. This points to a CCID/PCSC path issue after long sessions;
  exact root cause is unresolved. The full supported sweep remains failed and
  the independent heap-retention OOM was not observed in this reproduction.
- Decision: Keep USB calls in host-access context. Do not retry the long sweep
  without new diagnostics or a code-level hypothesis; continue with CCID/PCSC
  state investigation and declared-support/reference comparison. ARM M33 only.
- Commit: `c6bec06`.
- Artifacts/logs:
  `artifacts/logs/jcalgtest-supported-arm-disconnect-repro-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-repro-20261017.log`,
  `artifacts/logs/flash-arm-supported-disconnect-recovery-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-recovery-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-after-disconnect-reflash-20261017.log`.

## 2026-10-17 — Fail closed before restricted USB probes

- Step/experiment: `host-usb-guard-fail-closed`
- Change: Shell and Python USB/PCSC guards now require a readable `/proc/self/status` explicitly reporting `NoNewPrivs: 0`; missing or malformed context data blocks access. Added a first-attempt host-access rule to `AGENTS.md` and clarified the physical-token procedure. Direct commands cannot be elevated by repository wrappers, so hardware operations must start in host access.
- Validation: `bash -n` passed for all shell wrappers; Python compilation passed. All eight USB/PCSC entry points rejected in the current restricted context in 0–1 seconds with the guard diagnostic. A simulated unreadable proc-status file also rejected before USB access. No libusb or PC/SC operation ran.
- Result: The recurring restricted-context delay is prevented for wrapper-based commands; direct commands are covered by explicit operating instructions. No 32-second probe was repeated or measured during this validation.
- Decision: Use host-access execution from the first hardware command. Continue the supported-sweep failure investigation offline until that context is selected.
- Commit: `13808f4`
- Artifacts/logs: No new logs; validation output was inspected directly.

## 2026-10-17 — Implement OwnerPINx behavior and fix inherited dispatch

- Step/experiment: `ownerpinx-dispatch-fields-regression`
- Change: Implemented both OwnerPINx variants and the builder results; corrected internal superclass method tokens and `invokespecial` resolution; mapped declaring-class field tokens into flattened inherited instance storage; preserved successful OwnerPIN validation when subclasses override `reset()`. Removed temporary field-bytecode instrumentation. Added VPCD cases 12–13 and wrote the reusable causes to `.agent/QKB.md`.
- Validation: Full VPCD regression passed `TEST:SUMMARY pass=16 fail=0 elapsed_s=59.295`. After diagnostic cleanup, focused case 13 passed with response `030302000100000202000201030103009000` and `TEST:SUMMARY pass=3 fail=0`. PC and ARM builds passed. The PC build emitted 49 existing Rust javacard-os warnings and two packed-member alignment warnings; ARM build had no warnings. No hardware was accessed.
- Result: OwnerPINx try-limit, blocked-PIN, predecrement, successful validation, reset, superclass dispatch, and inherited-field behavior pass through the applet. This does not resolve static benchmark heap retention.
- Decision: Continue from the separate unresolved static benchmark and PC/SC reader-path findings. Keep physical access out of this step.
- Commit: root `63a13eb`; nested API `0199d51`; nested rommask `051b389` and `f3c309a`. This SHA was filled in by the follow-up progress-record commit.
- Artifacts/logs: `artifacts/logs/jcsystem-ownerpinx-full-vpcd-20261017.log`, `artifacts/logs/jcsystem-ownerpinx-cleanup-vpcd-20261017.log`, `artifacts/logs/build-pc-ownerpinx-cleanup-20261017.log`, `artifacts/logs/build-arm-ownerpinx-cleanup-20261017.log`.

## 2026-10-17 — Clarify OwnerPINx support validation limits

- Step/experiment: `ownerpinx-official-support-vpcd-attempt`
- Change: Updated TODO, QKB, and handoff to mark OwnerPINx behavior implemented while keeping the post-change physical support sweep open. Documented the VPCD startup delay so it is not mistaken for an OwnerPINx failure. Removed the unvalidated support-query additions to the generic VPCD harness.
- Validation: Confirmed the focused OwnerPINx case 13 still passes from the preceding cleanup run. Official JCAlgTest VPCD reached SELECT, then GETVERSION timed out after 20 seconds and again after 90 seconds; a debug run timed out after 30 seconds and traced the VM in JavaCardAES S-box generation. No OwnerPINX support APDU was reached. Rebuilt PC VM with `CHOUPI_JCVM_DEBUG=OFF`; build passed. No hardware was accessed.
- Result: The applet's behavior is covered by the focused VPCD case, while official capability-row comparison remains unverified after implementation. The expensive delayed applet setup is a separate desktop VPCD limitation.
- Decision: Do not claim the physical support comparison complete. Continue the separate static benchmark/reader-path investigation; keep the board untouched.
- Commit: `1ed8d0f` (this SHA was filled by the follow-up progress-record commit).
- Artifacts/logs: `artifacts/logs/pc-vpcd-ownerpinx-support-20261017.log`, `artifacts/logs/pc-vpcd-ownerpinx-support-retry-20261017.log`, `artifacts/logs/pc-vpcd-ownerpinx-debug-20261017.log`, `artifacts/logs/build-pc-ownerpinx-support-debug-20261017.log`, `artifacts/logs/build-pc-ownerpinx-support-release-20261017.log`.

## 2026-10-17 — Diagnose supported-sweep heap exhaustion

- Step/experiment: `supported-sweep-retained-exceptions`
- Change: Corrected `.agent/QKB.md`, `TODO.md`, and `.agent/HANDOFF.md`: the supported-sweep monitor tail does contain `malloc 12 failed to allocate memory`. Source inspection found that `CryptoException.throwIt()` allocates per call and `Heap::addInstance()` retains instances without object reclamation. Tested, then reverted, a Java static exception cache because VM static references persist through flash serialization and did not preserve object identity.
- Validation: Rechecked the physical-run log at line 62,594. The lazy-static experiment regenerated the PC ROM mask and ARM build with no converter errors; its 50-throw VPCD probe returned a distinct instance at throw 12 (`02010C00`) and failed the intended identity assertion. All pre-existing VPCD transaction, SHA-256, JCSystem, and unsupported-digest cases before that new assertion passed. No source change from this experiment remains.
- Result: The sweep's allocation failure is explained by cumulative retained instances, likely including repeated `CryptoException` allocations. Exact allocation-to-exception counts were not measured. Persistent Java statics are not a valid cache for JCRE-owned exception objects.
- Decision: Implement a VM-managed per-context runtime exception root or correct object reclamation, then pass a repeated-throw test before another physical supported sweep. Continue treating unsupported algorithms as expected.
- Commit: `413828b`.
- Artifacts/logs: `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log`, `artifacts/logs/build-pc-exception-reuse-v2-20261017.log`, `artifacts/logs/build-arm-exception-reuse-v2-20261017.log`, and `artifacts/logs/jcsystem-exception-reuse-vpcd-v2-20261017.log`.

## 2026-10-17 — Require an explicit host-access latch for USB commands

- Step/experiment: `host-usb-explicit-context-latch`
- Change: Added a required per-command `PICO2_HOST_USB_ACCESS=1` marker to shell and Python USB/PCSC guards, while retaining fail-closed `NoNewPrivs: 0` checks. Clarified in `AGENTS.md` and the physical-token reference that wrappers require both the marker and `sandbox_permissions: "require_escalated"`; direct USB commands must be escalated from the first invocation. Updated QKB and handoff notes.
- Validation: Shell syntax checks and Python compilation passed. With the marker absent, shell and Python guards rejected in 72 ms and 96 ms. With the marker set but current `NoNewPrivs: 1`, the shell wrapper rejected in 90 ms. No libusb, PC/SC, or physical-device command was run.
- Result: A forgotten escalation request can no longer flow from a repository wrapper into a delayed USB call, even if `NoNewPrivs` alone is misleading. Direct system commands still rely on the explicit tool-level escalation rule.
- Decision: Use the marker only on individual host-access tool calls; if escalation is unavailable, continue offline.
- Commit: `102764f` (implementation/instructions); progress record committed separately.
- Artifacts/logs: No new logs; validation output was inspected directly.

## 2026-10-17 — Record safe heap-reclamation constraints

- Step/experiment: `heap-reclamation-root-architecture-review`
- Change: Documented why list erasure/compaction invalidates 1-based `jref_t` handles, why collection during interpreter/native execution cannot see all references, and why an empty APDU stack still needs static, context, persistent-field, and reference-array roots. Recorded stable-slot reuse and root-enumeration as prerequisites in `.agent/QKB.md` and `.agent/HANDOFF.md`.
- Validation: Source inspection of `Heap`, `Stack`, `Context`, `JC_Instance`, `JC_Array`, and static-field handlers. No source code changed; no build, runtime test, or hardware action was run.
- Result: A per-APDU heap clear or list compaction is unsafe. The static SHA cache fix resolves its measured heap growth but does not implement general collection.
- Decision: Next add canonical VM roots for static references and explicit root enumeration; defer sweep/reuse until typed object-graph tracing covers all persistent roots.
- Commit: `76f4a71`.
- Artifacts/logs: none.

## 2026-10-17 — Identify repeated static-reference reconstruction

- Step/experiment: `static-reference-root-source-review`
- Change: Traced `GETSTATIC_A` through `bc_getstatic_a()` and confirmed it bypasses `Static_Handler::getPersistentRef()` and calls `getPersistentField_Reference()` on every access. Recorded that serialized object/array references are reconstructed as new heap entries, losing canonical reference identity and retaining duplicates; added cache-key and invalidation constraints to `.agent/QKB.md` and `.agent/HANDOFF.md`.
- Validation: Read-only source inspection of static get/put bytecodes, `Static_Handler`, and persistent-reference deserialization. No source code changed; no build, runtime test, or hardware action was run.
- Result: A static root cache is necessary both for Java reference identity and future safe heap collection. The cache must be keyed by resolved package/static field and invalidated with lifecycle deletion/context teardown.
- Decision: Next trace install/delete/context lifecycle ownership, then implement canonical static reference roots with focused identity coverage before starting a collector.
- Commit: `c6bec06`.
- Artifacts/logs: none.

## 2026-10-17 — Finalize heap review commit records

- Step/experiment: `heap-review-progress-finalization`
- Change: Replaced pending commit markers with `76f4a71` and `c6bec06` for the two completed architecture/source-review steps.
- Validation: `git diff --check` passed for `.agent/QKB.md`, `.agent/HANDOFF.md`, and `progress.md`; both commits contain only the review notes and progress records.
- Result: The review trail now points to its committed documentation changes.
- Decision: Continue next with package/app deletion and context lifecycle tracing before implementing static-reference caching.
- Commit: `af36522` (this SHA was filled in by the follow-up progress-record commit).
- Artifacts/logs: none.
## 2026-10-17 — Canonicalize restored static reference roots

- Step/experiment: `static-reference-root-cache`
- Change: Routed `GETSTATIC_A` through `Static_Handler::getPersistentRef` and added a per-heap cache keyed by resolved package and field. Successful persistent reference writes refresh the cache. Added transaction-probe case 14 to verify repeated reads of an install-written static object preserve identity after restore. Recorded the root cause and lifecycle behavior in `.agent/QKB.md` and `.agent/HANDOFF.md`.
- Validation: `./tools/build-pc` passed; `./tools/build-jcsystem-transaction-probe` passed; focused VPCD case 14 passed with `019000` (`TEST:SUMMARY pass=3 fail=0 elapsed_s=6.819`); full VPCD regression passed (`TEST:SUMMARY pass=17 fail=0 elapsed_s=109.117`); `./tools/build arm` passed. No hardware or RISC-V validation was run.
- Result: Repeated static reference reads return one canonical heap handle and no longer allocate duplicate restored wrappers. This is not heap reclamation.
- Decision: Commit the static-reference cache and regression independently. Any future transactional static-field rollback must reconcile this cache on abort.
- Commit: `425602b`
- Artifacts/logs: `artifacts/logs/build-pc-static-reference-cache-20261017.log`, `artifacts/logs/build-jcsystem-static-reference-cache-20261017.log`, `artifacts/logs/jcsystem-static-reference-cache-full-vpcd-20261017.log`, `artifacts/logs/build-arm-static-reference-cache-20261017.log`.

## 2026-10-17 — Inventory roots for safe heap reclamation

- Step/experiment: `heap-root-inventory-after-static-cache`
- Change: Traced context-held references, heap-owned references, typed instance fields, reference-array entries, lazy persistent deserialization, interpreter/native stack lifetimes, and offset-based handle storage. Updated `.agent/QKB.md` and `.agent/HANDOFF.md` to supersede the earlier pre-cache root assessment and record the safe mark-only sequence.
- Validation: Read-only source inspection of `Context`, `Heap`, `Stack`, `Frame`, `JC_Instance`, `JC_Array`, `FlashMemory_Handler`, and APDU dispatch. No source code changed; no build, runtime test, or hardware check was run.
- Result: Static roots are cached but not enumerable. Context refs and lazy typed graph edges also need visitor support. Heap slots cannot be erased or compacted; run a complete mark-only traversal before enabling tombstone reuse.
- Decision: Add explicit root/edge visitors and validate marking across APDU boundaries before implementing reclamation.
- Commit: `6d130ae`
- Artifacts/logs: none.

## 2026-10-18 — Expose VM roots and typed heap edges

- Step/experiment: `heap-root-edge-visitors`
- Change: Added heap visitors for runtime/static roots and typed instance/reference-array children. Added a context root visitor for applet, AID, APDU handles and retained APDU array mirrors. The visitors only enumerate; they do not free objects.
- Validation: `./tools/build-pc`, `./tools/build arm`, and `./tools/build riscv` passed. Initial ARM build caught a new `-Werror=range-loop-construct`; changed the loop variable to a const reference and rebuilt successfully. Full VPCD transaction/JCSystem regression passed `TEST:SUMMARY pass=17 fail=0 elapsed_s=110.271`. No board flash or hardware check was run. PC build emitted pre-existing Rust and packed-member warnings; final ARM build had no warnings.
- Result: Context/heap roots and typed object/array edges can now be enumerated. Persistent edge reads may allocate heap wrappers, so the next mark walk must use a dynamically growing work queue. Stack words/native locals are still outside this visitor API.
- Decision: Add mark-only reachability traversal at a post-APDU quiescent point; defer all slot reclamation and reuse until completeness is demonstrated.
- Commit: `cb17037`
- Artifacts/logs: `artifacts/logs/build-pc-heap-root-visitors-20261018.log`, `artifacts/logs/build-arm-heap-root-visitors-20261018.log`, `artifacts/logs/build-riscv-heap-root-visitors-20261018.log`, `artifacts/logs/jcsystem-heap-root-visitors-vpcd-20261018.log`.

## 2026-10-18 — Add mark-only typed heap traversal

- Step/experiment: `heap-mark-only-traversal`
- Change: Added `Heap::traceReachableFrom()` to mark and count array/instance
  handles reachable from a supplied root set. It uses a deduplicating
  work queue, validates offsets, follows the typed edge visitor, and grows
  mark vectors when persistent reads append objects. Invalid handles and VM
  edge-load exceptions make the result incomplete; no slots are freed.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. The full VPCD JCSystem/transaction regression
  passed `TEST:SUMMARY pass=17 fail=0 elapsed_s=70.499`. No hardware was
  flashed. PC build emitted the existing packed-member warnings.
- Result: Mark-only traversal compiles and does not regress the applet suite.
  It is not wired into runtime code and has not yet been measured against a
  complete root set; these counts are not safe for reclamation.
- Decision: Expose remaining VM roots and establish a quiescent post-APDU
  invocation before considering any slot reclamation or reuse.
- Commit: `a7f1f58`.
- Artifacts/logs: `artifacts/logs/build-pc-heap-mark-only-20261018.log`,
  `artifacts/logs/build-arm-heap-mark-only-20261018.log`,
  `artifacts/logs/build-riscv-heap-mark-only-20261018.log`, and
  `artifacts/logs/jcsystem-heap-mark-only-vpcd-20261018.log`.

## 2026-10-18 — Enumerate runtime CryptoException roots

- Step/experiment: `heap-runtime-exception-roots`
- Change: Added per-heap reusable handles for ROM and imported
  `CryptoException` views, routed matching `NEW` factory allocations to those
  handles, and made `Heap::appendRootReferences()` enumerate them with static
  references.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. VPCD JCSystem regression passed
  `TEST:SUMMARY pass=17 fail=0 elapsed_s=71.327`, including the 50-throw
  identity/reason case. No hardware was flashed.
- Result: Runtime exception objects are reused per view and visible to the
  heap root visitor. The tracer still is not invoked in runtime code; other
  stack/native roots remain unresolved.
- Decision: Inspect the root-return cache's typing/lifetime and establish a
  safe post-APDU diagnostic mark point before considering reclamation.
- Commit: `352d709`.
- Artifacts/logs: `artifacts/logs/build-pc-heap-runtime-roots-20261018.log`,
  `artifacts/logs/build-arm-heap-runtime-roots-20261018.log`,
  `artifacts/logs/build-riscv-heap-runtime-roots-20261018.log`, and
  `artifacts/logs/jcsystem-heap-runtime-roots-vpcd-20261018.log`.

## 2026-10-18 — Preserve typed top-level reference return roots

- Step/experiment: `typed-root-return-value`
- Change: Added a type tag for the top-level stack return cache. `ARETURN`
  stores a typed `jref_t`; primitive return setters clear the tag and a new
  root frame resets stale return state. `Context::appendRootReferences()` now
  includes the cached reference return when present.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. VPCD JCSystem regression passed
  `TEST:SUMMARY pass=17 fail=0 elapsed_s=70.323`. No hardware was flashed.
- Result: The root visitor can distinguish cached references from primitive
  top-level return values. Active Java frames and native locals remain
  unenumerated.
- Decision: Verify the APDU call boundary and establish a quiescent point
  before invoking the tracer; do not reclaim slots yet.
- Commit: `0f2f64b`.
- Artifacts/logs: `artifacts/logs/build-pc-typed-root-return-20261018.log`,
  `artifacts/logs/build-arm-typed-root-return-20261018.log`,
  `artifacts/logs/build-riscv-typed-root-return-20261018.log`, and
  `artifacts/logs/jcsystem-typed-root-return-vpcd-20261018.log`.

## 2026-10-18 — Add opt-in post-APDU heap diagnostics

- Step/experiment: `post-apdu-heap-trace-diagnostic`
- Change: Added a PC-only `PICO2_HEAP_TRACE=1` hook after the APDU try scope
  has released its interpreter/native locals. It requires an empty Java stack,
  collects Context roots, and reports mark totals without reclaiming slots.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. VPCD regression passed 17/17 in 73.382 s with
  tracing enabled. All 15 trace samples were complete, with zero invalid
  handles, zero edge-load failures, and no active-frame skips. Final sample:
  arrays 6/16 reachable, instances 4/12 reachable. No hardware was used.
- Result: The typed visitors and mark walk run at a host-side post-APDU
  boundary and tolerate lazy heap growth. Remaining roots still need audit;
  unreachable slots are not yet safe to reclaim.
- Decision: Audit persistent/runtime root paths and add a focused completeness
  test before designing tombstones or reuse.
- Commit: `88077d8`.
- Artifacts/logs: `artifacts/logs/build-pc-heap-trace-diagnostic-20261018.log`,
  `artifacts/logs/build-arm-heap-trace-diagnostic-20261018.log`,
  `artifacts/logs/build-riscv-heap-trace-diagnostic-20261018.log`, and
  `artifacts/logs/jcsystem-heap-trace-diagnostic-vpcd-20261018.log`.

## 2026-10-18 — Check for allocations caused by repeated tracing

- Step/experiment: `heap-trace-repeat-stability`
- Change: Added a bounded 1–4 pass count to the opt-in PC heap diagnostic so
  consecutive graph walks can reveal deserialization or cache growth.
- Validation: PC, ARM, and RISC-V builds passed. VPCD JCSystem regression
  passed `TEST:SUMMARY pass=17 fail=0 elapsed_s=96.618` with two walks per
  APDU. All 30 trace results were complete, with zero invalid references and
  zero edge-load failures; no heap-total changes appeared between the 15
  consecutive pass pairs. No hardware was used.
- Result: Repeat tracing did not grow the exercised VPCD heap graph. This
  does not cover every persistent reference-array or alias shape.
- Decision: Add focused identity/root tests for persistent arrays and aliases
  before considering tombstones or reuse. Keep repeat tracing opt-in.
- Commit: `926c14a`.
- Artifacts/logs: `artifacts/logs/build-pc-heap-trace-repeat-20261018.log`,
  `artifacts/logs/build-arm-heap-trace-repeat-20261018.log`,
  `artifacts/logs/build-riscv-heap-trace-repeat-20261018.log`, and
  `artifacts/logs/jcsystem-heap-trace-repeat-vpcd-20261018.log`.
## 2026-10-18 — Canonicalize persistent reference-array children

- Step/experiment: `persistent-array-child-identity`
- Change: Persisted arrays now retain their canonical flash tag. Repeated
  persistent references encode an array alias and resolve through heap lookup
  by persistent tag. Added case 15 for two entries referencing one shared
  byte array.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused case 15 passed (`019000`, 3/3 checks).
  Full VPCD JCSystem regression with `PICO2_HEAP_TRACE=2` passed
  `TEST:SUMMARY pass=18 fail=0 elapsed_s=81.386`. All 36 trace walks completed
  with zero invalid refs and zero edge-load failures. No hardware was used.
- Result: The test reproduced false reference identity before the fix and
  passed after alias canonicalization. This covers the tested array-child
  graph only; cyclic arrays and other persistent layouts remain unverified.
- Decision: Keep reclamation disabled. Expand alias/root coverage before any
  tombstone or stable-slot reuse design.
- Commit: `f42982d`.
- Artifacts/logs: `artifacts/logs/build-pc-persistent-array-identity-final-20261018.log`,
  `artifacts/logs/build-arm-persistent-array-identity-final-20261018.log`,
  `artifacts/logs/build-riscv-persistent-array-identity-final-20261018.log`,
  `artifacts/logs/jcsystem-persistent-array-identity-fix-vpcd-20261018.log`,
  and `artifacts/logs/jcsystem-persistent-array-identity-full-vpcd-20261018.log`.


## 2026-10-18 — Verify aliases in persistent instance fields

- Step/experiment: `persistent-instance-array-alias-identity`
- Change: Extended the transaction applet probe with a persisted holder whose
  two `byte[]` fields point to the same array. The APDU checks reference
  identity and mutation visibility across the two fields.
- Validation: The repository test wrapper compiled the CAP and focused VPCD
  case 16 passed (`019000`, 3/3 checks). Full VPCD regression with
  `PICO2_HEAP_TRACE=2` passed `TEST:SUMMARY pass=19 fail=0 elapsed_s=85.020`.
  All 34 trace walks completed with zero invalid refs and zero edge-load
  failures. No C++ source changed; ARM/RISC-V rebuild and hardware access were
  intentionally skipped.
- Result: Alias identity and shared mutation work through persisted object
  fields in this tested shape. The initial `Object`-typed version failed at
  `checkcast` (`0x94`) with `ClassCastException`; that separate behavior is
  untriaged and recorded in QKB.
- Decision: Keep reclamation disabled. Isolate the cast failure and continue
  broader root/alias coverage before designing tombstones.
- Commit: `b6db227`.
- Artifacts/logs: `artifacts/logs/jcsystem-persistent-object-array-alias-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-persistent-object-array-alias-vpcd-v2-20261018.log`,
  `artifacts/logs/jcsystem-persistent-object-array-alias-full-vpcd-20261018.log`.

## 2026-10-18 — Fix Java Card array checkcast type mapping

- Step/experiment: `checkcast-array-type-code-map`
- Change: Replaced the direct bytecode-to-`jc_array_type` cast in `docheck()`
  with explicit mappings for array `atype` codes 10–14. Added case 17 to cast
  a restored `byte[]` reached through `Object[]`.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused case 17 passed (`019000`); cases 16 and
  17 also passed in order (`019000` each). Full VPCD suite passed
  `TEST:SUMMARY pass=20 fail=0 elapsed_s=114.250`. All 36 heap trace walks
  completed with zero invalid refs and zero edge-load failures. No board was
  accessed.
- Result: Valid byte-array casts now pass. The first full run failed only
  because case 17 expected the original array byte after case 16 intentionally
  mutated it; the probe now checks array length. Negative mismatched-type casts
  remain untested.
- Decision: Keep the explicit atype mapping and add a negative cast case next.
- Commit: `678655a`.
- Artifacts/logs: `artifacts/logs/build-pc-checkcast-array-type-map-20261018.log`,
  `artifacts/logs/build-arm-checkcast-array-type-map-20261018.log`,
  `artifacts/logs/build-riscv-checkcast-array-type-map-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-byte-array-from-persistent-object-array-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-byte-array-type-map-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-array-type-map-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-array-type-map-full-vpcd-20261018.log`.

## 2026-10-18 — Dispatch and reuse ClassCastException

- Step/experiment: `catchable-runtime-classcast-exception`
- Change: A failing array `checkcast` now resolves the `java.lang` imported
  ClassCastException class, obtains a per-heap JCRE-owned exception instance,
  and calls the normal Java exception-table dispatcher. Heap root enumeration
  retains this runtime-owned object. Case 18 performs 50 mismatched casts and
  checks they are caught using one stable exception instance.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused 50-cast case passed (`019000`). Full
  VPCD regression with `PICO2_HEAP_TRACE=2` passed
  `TEST:SUMMARY pass=21 fail=0 elapsed_s=110.937`. All 38 walks completed with
  zero invalid refs and zero edge-load failures. Final trace: arrays 6/16,
  instances 6/14. No board was accessed.
- Result: Negative array casts are caught by Java and repeated failures reuse
  one VM-owned exception object. Other VM-generated runtime exceptions may
  still bypass Java exception-table dispatch.
- Decision: Audit other VM-generated exception paths and continue graph/root
  coverage; keep reclamation disabled.
- Commit: `f91a9b5`.
- Artifacts/logs: `artifacts/logs/build-pc-catchable-classcast-20261018.log`,
  `artifacts/logs/build-arm-catchable-classcast-20261018.log`,
  `artifacts/logs/build-riscv-catchable-classcast-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-positive-negative-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-exception-50-reuse-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-checkcast-exception-50-reuse-full-vpcd-20261018.log`.

## 2026-10-18 — Dispatch VM runtime exceptions through Java handlers

- Step/experiment: `catchable-vm-runtime-exceptions`
- Change: The interpreter now maps supported VM-raised runtime faults to
  reusable, heap-rooted Java exception instances and invokes the normal
  exception-table search. Added catchable cases for null array access, array
  bounds, division by zero, and negative array size. Added logical-length
  checks to all typed `JC_Array` read/write entry points so out-of-range access
  raises `ArrayIndexOutOfBoundsException` instead of leaking the generic
  container exception.
- Validation: PC, ARM M33, and RISC-V builds passed. Focused VPCD cases 19–22
  passed (`019000` each), as did the 50-cast ClassCastException regression.
  Full VPCD suite with `PICO2_HEAP_TRACE=2` passed
  `TEST:SUMMARY pass=25 fail=0 elapsed_s=104.317`; all 46 trace walks were
  complete with zero invalid refs and zero edge-load failures. No board was
  accessed.
- Result: Caught NPE, AIOOBE, ArithmeticException, NegativeArraySizeException,
  and ClassCastException execute through Java handlers. Initial case 20
  exposed the incorrect generic bounds exception; the logical-length checks
  fixed it. Unsupported/unmapped VM exceptions remain fatal by design.
- Decision: Continue auditing other VM exception sources and root paths. Keep
  heap reclamation disabled; `NO_SUCH_ALGORITHM` remains expected for absent
  algorithms.
- Commit: `022c080`.
- Artifacts/logs: `artifacts/logs/build-{pc,arm,riscv}-vm-runtime-exception-dispatch-20261018.log`,
  `artifacts/logs/jcsystem-runtime-exception-dispatch-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-runtime-exception-dispatch-mappings-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-runtime-exception-dispatch-full-vpcd-20261018.log`.

## 2026-10-18 — Dispatch ArrayStoreException

- Step/experiment: `catchable-array-store-exception`
- Change: Mapped internal `ArrayStoreException` to Java `java.lang` export token
  11 and added a probe that stores the applet instance into an `Exception[]`
  through an `Object[]` reference, exercising a VM-raised type mismatch.
- Validation: PC, ARM M33, and RISC-V builds passed. Focused VPCD case 23 passed
  (`019000`). Full suite with two-pass heap tracing passed
  `TEST:SUMMARY pass=26 fail=0 elapsed_s=116.840`; all 48 walks were complete
  with zero invalid refs and zero edge-load failures. No physical board was
  accessed.
- Result: The checked-in JC 3.0.5 `java/lang/javacard/lang.jca` identifies
  `ArrayStoreException` as export token 11. Java catches the runtime exception.
  A separate `new Object()` probe terminated the PC emulator before the store;
  its cause is unresolved and recorded in QKB.
- Decision: Keep the ArrayStoreException mapping. Continue the remaining VM
  exception audit and separately investigate `new java.lang.Object()`.
- Commit: `3cc4c61`.
- Artifacts/logs: `artifacts/logs/build-{pc,arm,riscv}-arraystore-dispatch-final-20261018.log`,
  `artifacts/logs/jcsystem-arraystore-this-probe-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-arraystore-dispatch-full-vpcd-20261018.log`.

## 2026-10-18 — Dispatch JCSystem SystemException

- Step/experiment: `catchable-jcsystem-system-exception`
- Change: Generalized VM exception resolution to select a package by AID and
  class export token. Mapped internal `SystemException` to
  `javacard.framework` token 13 and set reason field 0 to `ILLEGAL_VALUE` for
  the invalid `JCSystem.getAvailableMemory(byte)` type path. Added case 24 to
  verify Java catch behavior and the reason code.
- Validation: PC, ARM M33, and RISC-V builds passed. Focused VPCD case 24
  passed (`019000`). Full suite with two-pass heap tracing passed
  `TEST:SUMMARY pass=27 fail=0 elapsed_s=98.571`; all 50 trace walks completed
  with zero invalid refs and zero edge-load failures. No board was accessed.
- Result: The VM-raised SystemException is caught by the applet, and
  `getReason()` returns `SystemException.ILLEGAL_VALUE` (1).
- Decision: Continue auditing VM-raised Java Card framework exceptions.
  Exception values that lack a known reason mapping remain unmodified. Keep
  heap reclamation disabled.
- Commit: `26583e2`.
- Artifacts/logs: `artifacts/logs/build-{pc,arm,riscv}-jcsystem-systemexception-20261018.log`,
  `artifacts/logs/jcsystem-systemexception-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-systemexception-full-vpcd-20261018.log`.

## 2026-10-18 — Instantiate java.lang.Object safely

- Step/experiment: `java-lang-object-root-class-construction`
- Change: Fixed `Class_Handler::getInstanceFieldsSize()` to stop at the root
  class before resolving a superclass, and removed `noexcept` from
  `JC_Instance` constructors that perform CAP lookup and field sizing. Added
  case 25 to instantiate `new Object()` and check the result.
- Validation: PC, ARM M33, and RISC-V builds passed. Focused VPCD case 25 passed
  (`019000`). Full suite with two-pass heap tracing passed
  `TEST:SUMMARY pass=28 fail=0 elapsed_s=102.270`; all 52 trace walks completed
  with zero invalid refs and zero edge-load failures. No physical board was
  accessed.
- Result: `new java.lang.Object()` no longer resolves Object's nonexistent
  superclass or terminates the emulator through a `noexcept` constructor.
  Earlier ArrayStoreException test failure was caused by this construction
  path; case 23 now uses the applet instance and still passes.
- Decision: Continue auditing VM exception and allocation paths. Keep heap
  reclamation disabled.
- Commit: `f2b33f8`.
- Artifacts/logs: `artifacts/logs/build-{pc,arm,riscv}-java-lang-object-construction-20261018.log`,
  `artifacts/logs/jcsystem-java-lang-object-construction-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-java-lang-object-construction-full-vpcd-20261018.log`.

## 2026-10-18 — Dispatch VM-raised APDUException

- Step/experiment: `apdu-exception-handler-dispatch`
- Change: Mapped the VM's internal APDU state exception to the imported Java
  Card framework `APDUException` and populated its reason with
  `ILLEGAL_USE`. Added cases 26–27 to provoke a native APDU state violation,
  catch it, and verify its reason on the next command.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused VPCD cases 26–27 passed. Full VPCD
  regression passed `TEST:SUMMARY pass=30 fail=0 elapsed_s=159.731`. No
  physical board was accessed.
- Result: Java applet code catches the APDU state exception and observes reason
  `1` (`APDUException.ILLEGAL_USE`). A prior test design received `6F00` while
  sending a response on the already-invalid APDU state; the corrected test
  returns from `process()` and checks the stored reason on the next APDU.
- Decision: Continue checking the remaining `JCSystem` transient-array
  operations and clear events. Heap reclamation stays disabled.
- Commit: `f42a2bc`.
- Artifacts/logs: `artifacts/logs/apdu-exception-dispatch-focused-vpcd-20261018.log`,
  `artifacts/logs/apdu-exception-dispatch-full-vpcd-20261018.log`.

## 2026-10-18 — Correct invalid JCSystem transient-array event handling

- Step/experiment: `jcsystem-transient-array-events`
- Change: Changed invalid transient clear-event values to throw the Java Card
  specified `SystemException.ILLEGAL_VALUE`. Added cases 28–29 covering all
  four transient array factories, event reporting, values, and invalid-event
  catch/reason behavior.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused VPCD cases 28–29 passed. Full VPCD
  suite passed `TEST:SUMMARY pass=32 fail=0 elapsed_s=113.436`. No hardware was
  accessed.
- Result: Byte, boolean, short, and object transient factories return arrays
  with correct element access and event values from `JCSystem.isTransient()`;
  invalid event 3 is caught with reason 1. This verifies API metadata and
  factory behavior, not reset/deselect clearing while a heap survives.
- Decision: Inspect the reset/deselect lifecycle before wiring the currently
  unused heap clear helper. Continue auditing JCSystem methods; keep heap
  reclamation disabled.
- Commit: `80b09fd`.
- Artifacts/logs: `artifacts/logs/jcsystem-transient-arrays-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-transient-arrays-full-vpcd-20261018.log`.

## 2026-10-18 — Preserve JCSystem transient-array lifetimes

- Step/experiment: `jcsystem-transient-array-lifecycle`
- Change: Retained the applet `Context` across same-AID reselection. After the
  applet's `deselect()` callback, clear `CLEAR_ON_DESELECT` arrays; on card
  reset or power-off, clear `CLEAR_ON_RESET` arrays. VPCD selection no longer
  invokes the reset clear callback. Added lifecycle cases 30–33.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused VPCD cases 28–33 passed. Full VPCD
  suite passed `TEST:SUMMARY pass=36 fail=0 elapsed_s=114.875`. No physical
  device was accessed.
- Result: Reset-event arrays survive applet deselection/reselection;
  deselect-event arrays clear on deselection; both clear after card reset.
- Decision: Continue auditing remaining JCSystem method stubs and lifecycle
  behavior. Cross-applet context caching remains unverified; heap reclamation
  stays disabled.
- Commit: `370a1bd`.
- Artifacts/logs: `artifacts/logs/jcsystem-transient-lifecycle-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-transient-lifecycle-full-vpcd-20261018.log`.

## 2026-10-18 — Honor unsupported JCSystem object-deletion requests

- Step/experiment: `jcsystem-object-deletion-unsupported`
- Change: `requestObjectDeletion()` now throws a catchable
  `SystemException.ILLEGAL_USE` (reason 6) when object deletion is unsupported.
  VM-raised SystemException reasons are carried by the active context and
  consumed/reset during Java exception dispatch. Added probe case 34.
- Validation: `./tools/build-pc`, `./tools/build arm`, `./tools/build riscv`,
  and `./tools/build-jcsystem-transaction-probe` passed. Focused VPCD case 34
  passed (`019000`). Full VPCD suite passed
  `TEST:SUMMARY pass=37 fail=0 elapsed_s=173.290`. No physical device was
  accessed. Builds still report pre-existing Rust SDK and packed CAP-member
  warnings.
- Result: `JCSystem.isObjectDeletionSupported()` reports false and the paired
  request now returns the specified catchable exception with reason 6.
- Decision: Continue auditing the remaining JCSystem stubs, beginning with
  `makeGlobalArray()` and `isAppletActive()`. Keep heap reclamation disabled.
- Commit: `288f61b`.
- Artifacts/logs: `artifacts/logs/jcsystem-object-deletion-unsupported-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-object-deletion-unsupported-full-vpcd-20261018.log`.

## 2026-10-18 — Implement single-context JCSystem.isAppletActive

- Step/experiment: `jcsystem-isappletactive-current-context`
- Change: Implemented current-app AID matching for `JCSystem.isAppletActive()`.
  The native method checks the AID object's encapsulated byte array against the
  registered/restored applet AID and returns false for null, malformed,
  unregistered, or nonmatching AIDs. Added case 35 for current and unknown AIDs.
- Validation: `./tools/build-pc`, `./tools/build arm`, `./tools/build riscv`,
  and `./tools/build-jcsystem-transaction-probe` passed. Focused VPCD case 35
  passed (`019000`). Full suite passed
  `TEST:SUMMARY pass=38 fail=0 elapsed_s=221.009`. No physical device was
  accessed. Existing Rust SDK and packed CAP-member warnings remain.
- Result: Current registered applet is active; an unknown valid AID is not.
  Multiple logical channels and cross-applet activity are not modeled.
- Decision: Review `makeGlobalArray()` lifecycle semantics before implementing
  that remaining JCSystem stub. Heap reclamation stays disabled.
- Commit: `3c7da1d`.
- Artifacts/logs: `artifacts/logs/jcsystem-isappletactive-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-isappletactive-full-vpcd-20261018.log`.

## 2026-10-18 — Audit JCSystem global-array implementation boundary

- Step/experiment: `jcsystem-makeglobalarray-design-audit`
- Change: Documented why `makeGlobalArray()` cannot be correctly implemented
  as an ordinary transient array: global arrays need cross-context visibility
  and must be rejected from applet static/instance fields and array components.
- Validation: Read-only inspection of the JC305 `JCSystem.java` contract and
  the `Context`, `Heap`, `JC_Array`, and handle/firewall implementation. No
  code, build, or runtime test was performed by design.
- Result: The current per-context heap and heap-local handles cannot express
  global ownership or the required store restriction.
- Decision: Design a JCRE-owned global array store and global-reference type,
  including reset clearing and store rejection, before implementing this API.
  Keep heap reclamation disabled.
- Commit: `b7d8136`.
- Artifacts/logs: none.

## 2026-10-18 — Implement JCSystem.makeGlobalArray

- Step/experiment: `jcsystem-makeglobalarray`
- Change: Added a runtime-wide global-array heap and tagged handles for
  boolean, byte, short, int, and Object arrays. Global arrays use
  `CLEAR_ON_RESET`, resolve through applet heaps, and cannot be stored in
  instance/static fields or array components. Object[] casts recognize the
  Object component; null entries work, while applet-owned object writes are
  rejected because shareable-object ownership is not implemented. Added cases
  36–41 and extended the case selector.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused global-array cases passed. The full
  VPCD JCSystem suite passed `TEST:SUMMARY pass=44 fail=0 elapsed_s=144.901`.
  Case 41 was rerun after adding Object[] null-entry coverage and passed. No
  physical device was accessed. Existing pinned Rust OS and packed CAP-member
  warnings remain.
- Result: Primitive/Object global-array creation, access, invalid arguments,
  transient classification, and reference-store restrictions pass simulator
  checks. Cross-applet calls and reset clearing through an externally retained
  global reference remain unverified because this runtime has one applet
  context and lacks shareable-object ownership.
- Decision: Continue the JCSystem/runtime API audit. Keep heap reclamation off
  and treat unsupported algorithm results as expected.
- Commit: `1c652bd`.
- Artifacts/logs: `artifacts/logs/jcsystem-global-array-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-global-array-full-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-global-array-object-vpcd-20261018.log`.

## 2026-10-18 — Implement documented SensitiveArrays unsupported behavior

- Step/experiment: `sensitive-arrays-unsupported-semantics-and-jc305-remap`
- Change: Implemented the documented exceptions for integrity-sensitive array
  APIs while the capability is false. Added JC305 framework method-token
  translation for SensitiveArrays (imported tokens 0–4 map to embedded 1–5).
  Added probe cases 42–47 for query, assertion, creation, clear, null, and
  reason-code behavior.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused VPCD cases 42–47 passed. Full suite
  passed `TEST:SUMMARY pass=50 fail=0 elapsed_s=138.900`. No physical device
  was accessed. Existing pinned Rust OS and packed CAP-member warnings remain.
- Result: Unsupported integrity-sensitive arrays now fail with catchable,
  API-specific Java Card exceptions rather than `NotYetImplemented`. Correct
  static method-token translation allows JC305 applets to reach these handlers.
- Decision: Continue auditing `getAppletShareableInterfaceObject()` and other
  remaining JCSystem behavior against the current single-context model.
- Commit: `d220fdb`.
- Artifacts/logs: `artifacts/logs/sensitive-arrays-focused-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-sensitive-arrays-full-vpcd-20261018.log`,
  `artifacts/logs/sensitive-arrays-case42-vpcd-20261018.log`.

## 2026-10-18 — Define single-applet shareable lookup fallback

- Step/experiment: `jcsystem-shareable-lookup-single-applet`
- Change: Replaced the fatal `NotYetImplemented` native with the Java Card
  `null` result for absent servers and the default `Applet` implementation.
  Added case 48 for null, unknown, and current applet AIDs.
- Validation: `./tools/build-pc`, the probe CAP build, `./tools/build arm`, and
  `./tools/build riscv` passed. Focused VPCD case passed
  `TEST:SUMMARY pass=3 fail=0 elapsed_s=7.688`; full suite passed
  `TEST:SUMMARY pass=51 fail=0 elapsed_s=138.095`. No physical device was
  accessed.
- Result: The probe applet's supported default lookup path no longer aborts the
  VM. Custom `getShareableInterfaceObject` callbacks remain unsupported because
  the runtime has no multi-applet registry or context-switch bridge.
- Decision: Continue JCSystem and applet runtime audit; preserve expected
  unsupported algorithms.
- Commit: `b34ed04`.
- Artifacts/logs: `artifacts/logs/jcsystem-shareable-default-vpcd-20261018.log`,
  `artifacts/logs/jcsystem-shareable-full-vpcd-20261018.log`.

## 2026-10-18 — Add high-level project status

- Step/experiment: `project-status-overview`
- Change: Added root `STATUS.md` summarizing the tested subset, major gaps,
  physical validation state, next priorities, and links to the runbook and
  detailed project records.
- Validation: Cross-checked the summary against `readme.md`, `plan.md`, the
  latest handoff, QKB, build wrappers, and the recorded VPCD/hardware results.
  No build, test, or hardware action was needed for this documentation-only
  change.
- Result: The repository now has a concise entry point for current project
  maturity and remaining work. Historical plan content is explicitly labeled.
- Decision: Keep this summary current when capability or validation status
  materially changes; retain `HANDOFF.md` for chronological session details.
- Commit: `b3e845a`.
- Artifacts/logs: `STATUS.md`.

## 2026-10-18 — Exclude global arrays from applet heap tracing

- Step/experiment: `heap-trace-global-array-classification`
- Change: Updated the mark-only heap tracer to ignore global-array handles
  when marking applet-local slots or traversing local heap children. Global
  arrays belong to the runtime-wide heap and use reset lifecycle handling.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc`
  passed. Focused VPCD JCSystem cases 36–41 passed with
  `PICO2_HEAP_TRACE=2` (`TEST:SUMMARY pass=8 fail=0`); all 12 trace passes were
  complete with zero invalid references and zero edge-load failures. The
  official JCAlgTest VPCD harness was attempted but timed out at GETVERSION
  after 20 seconds per APDU; it did not reach an algorithm query. No board was
  accessed. Existing Rust and packed-member warnings remain in the PC build.
- Result: Global-array offsets no longer falsely mark applet-local arrays in
  the diagnostic tracer. No heap slots are reclaimed or reused.
- Decision: Continue root completeness audit before implementing a sweep.
- Commit: `c9aeb9d`.
- Artifacts/logs: `artifacts/logs/heap-global-tracer-focused-vpcd-20261018.log`
  (SHA-256 `3ebc56b04665c3e0cf87b9e51a0456d6a8c9c7c2125dc05bdce12feb254c01e2`).

## 2026-10-18 — Conservatively pin loaded persistent heap objects

- Step/experiment: `heap-trace-persistent-tag-roots`
- Change: Added every loaded persistent-tagged array and instance to the heap
  trace roots. Static reference fields are lazy-loaded, so their cached root
  entries alone could miss a persistent object already present in the heap.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc`
  passed. JCSystem VPCD cases 14–16 passed (`TEST:SUMMARY pass=5 fail=0`);
  six repeated traces were complete with zero invalid references and zero
  edge-load failures. No physical device was accessed. Existing Rust and
  packed-member warnings remain in the PC build.
- Result: Loaded persistent objects and their typed descendants remain
  conservatively reachable even before a lazy static-field read. No heap
  reclamation or slot reuse is enabled.
- Decision: Complete the remaining native-root audit before implementing a
  stable-slot sweep; persistent objects will remain pinned until deletion is
  supported.
- Commit: `510fd14`.
- Artifacts/logs: `artifacts/logs/heap-persistent-roots-focused-vpcd-20261018.log`
  (SHA-256 `f2e776ef989199e8781811bad8d3a7acb1014e3566dddc1056e9b74c2e651a00`).

## 2026-10-18 — Reclaim unreferenced stable heap slots

- Step/experiment: `heap-stable-slot-reclamation`
- Change: Added a mark-and-sweep operation that reclaims only after a complete
  typed root walk. Reclaimed entries remain tombstones so live `jref_t` offsets
  do not move; future allocations reuse those entries. Firmware runs the sweep
  after APDU interpreter/native scopes return and skips collection when Java
  frames remain or tracing/root enumeration is incomplete. Persistent-tagged
  objects are conservatively pinned; global arrays are excluded. Added probe
  cases 49–54 for repeated array and object allocation batches.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc`
  passed. VPCD focused GC/root cases passed 17/17 in 96.701 s; all traces were
  complete with zero invalid references and zero edge-load failures, and the
  64-element batches reused slots. `./tools/cycle choupi` flashed the physical
  ARM target and reported `TEST:SUMMARY pass=1 fail=0`; a bounded 240-second
  serial capture showed `TEST:PASS choupi_runtime` and no heap-GC skip, panic,
  or allocation-failure marker. Physical JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` ran for the wrapper's 900-second limit, then exited
  124; 1,297 cipher measurements returned `6F00`, with no allocation failure,
  OOM, fault, or reader-unavailable status in the client log.
- Result: Stable-slot collection and repeated allocation work in VPCD. The
  physical long run did not expose heap exhaustion or reader loss, but its
  static matrix did not complete and the cipher `6F00` failures remain a
  separate unresolved issue. Loaded persistent objects remain pinned.
- Decision: Keep reclamation enabled with the complete-trace/empty-stack guards.
  Next diagnose the cipher factory `6F00` path and rerun the full static suite;
  later audit native-held roots and implement persistent deletion before
  claiming general garbage collection.
- Commit: `b7b069a`.
- Artifacts/logs: VPCD log
  `artifacts/logs/heap-collector-full-focused-vpcd-20261018.log`
  (SHA-256 `bdd4601530b538a6aa8e9d57bfbaca8b8a02a0d5ea801b994adc55e54895bc09`);
  physical cycle `artifacts/logs/cycle-choupi-20261018T121110Z.log`;
  physical client log
  `artifacts/results/physical-perf-static-heap-gc-20261018/ALGTEST_log_1792325580379.log`
  (SHA-256 `31fc35f16fafac2709ef2ede586c2a14031aff0afa0a63f22195ce8eaea93847`);
  results CSV SHA-256
  `33d85895a7e18872ac44bfb7753797e782b4f5e15b89e81821306cb1eb8bf150`.

## 2026-10-18 — Remap the JC305 DESKey interface token

- Step/experiment: `jc305-deskey-interface-token-map`
- Change: Added the JC305 security-package interface mapping for `DESKey`
  (interface token 10 maps to generated CHOUPI token 12). Kept interface
  reference mapping separate from interface method-token mapping.
- Validation: ARM, RISC-V, and PC build wrappers passed. The exact physical
  `B0 31` DES Cipher prepare first returned `6F00` with `SecurityException`
  29. Reflashing with heap collection temporarily bypassed reproduced the same
  exception, ruling out the collector. With the token mapping and normal
  collection restored, the physical cycle passed and the focused probe passed:
  SELECT `9000`, GETVERSION `1.8.2_jc305`, JCSystem info `9000`, DES prepare
  `AA9000`. Bounded serial capture showed no applet exception during the probe.
- Result: The DES Cipher prepare failure was an imported interface-identity
  mismatch, not an unsupported algorithm result and not a heap-sweep failure.
  The full physical static matrix remains to be rerun.
- Decision: Rerun `ALG_PERFORMANCE_STATIC` on the corrected image, classify
  later failures independently, and preserve expected `NO_SUCH_ALGORITHM`.
- Commit: `278322e`.
- Artifacts/logs: `artifacts/logs/probe-des-interface-token-fix-20261018.log`
  (SHA-256 `f6e784fdb1038d011924351d670d5a7510bbb8704e3fb9881d604aebad7ad1fa`);
  serial capture `artifacts/logs/monitor-des-interface-token-fix-20261018.log`
  (SHA-256 `8a1bbffacf43e8a33721c721f126aab93c905a5c7bef616a82444d4611b97e55`);
  physical cycle `artifacts/logs/cycle-choupi-20261018T125316Z.log`; ARM,
  RISC-V, and PC builds are recorded in
  `artifacts/logs/build-arm-des-interface-token-fix-20261018.log`,
  `artifacts/logs/build-riscv-des-interface-token-fix-20261018.log`, and
  `artifacts/logs/build-pc-des-interface-token-fix-20261018.log`. The no-GC
  comparison logs are `artifacts/logs/probe-gc-disabled-des-cipher-20261018.log`
  and `artifacts/logs/monitor-gc-disabled-des-probe-20261018.log`.

## 2026-10-18 — Bound serial-port configuration retries

- Step/experiment: `bounded-monitor-stty-open`
- Change: Wrapped every `stty` attempt in `tools/monitor` with a one-second
  timeout and one-second kill grace period. Opening a stale ACM device had
  blocked the physical cycle indefinitely after flash.
- Validation: `bash -n tools/monitor` passed. `./tools/monitor --seconds 8`
  completed and captured repeated `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. The subsequent collector-enabled
  `./tools/cycle choupi` completed with `CYCLE:PASS`.
- Result: Serial setup retry and finite capture are now bounded even if a USB
  ACM node blocks during re-enumeration.
- Decision: Keep per-attempt timeouts; continue using host-access authorization
  for hardware and a short timeout for PC/SC scans.
- Commit: `d386520`.
- Artifacts/logs: `artifacts/logs/monitor-gc-disabled-diagnostic-20261018.log`
  (finite capture); collector-enabled cycle
  `artifacts/logs/cycle-choupi-20261018T125316Z.log`.

## 2026-10-18 — Complete physical JCAlgTest static matrix and isolate catch failure

- Step/experiment: `physical-static-after-deskey-map`
- Change: Archived the complete physical JCAlgTest 1.8.2 static run and isolated
  its first DES CBC ISO9797 M1 prepare failure with a single APDU and serial
  capture. Updated STATUS, QKB, and handoff with findings and next priority.
- Validation: Official `ALG_PERFORMANCE_STATIC` completed on the physical Pico
  2 with 7,785 CSV rows and no reader disconnect. SHA-256 and DES CBC NOPAD
  returned measurements; 40 unsupported probes reported `NO_SUCH_ALGORITHM`.
  There were 1,852 `CANT_BE_MEASURED` results. After a PC/SC reset, the isolated
  M1 prepare still returned `6F00`; serial showed exception 29 at `ATHROW`.
  No code or firmware was changed in this documentation/evidence step.
- Result: The DESKey interface fix resolved DES CBC NOPAD. Unsupported padding
  variants still fail because the applet's `CryptoException` catch does not
  complete normally. This is VM exception-dispatch behavior, not a request to
  implement those algorithms. The full static client completed but did not
  pass.
- Decision: Diagnose the catch-class/range path in `Bytecodes::doThrow()` and
  add a focused regression before the next physical matrix run. Keep expected
  `NO_SUCH_ALGORITHM` behavior.
- Commit: pending
- Artifacts/logs: client log SHA-256
  `3d869a23c9350a02839d0e5f10a948cf110365ae7de17a3c11305904bf74c1e0`; CSV
  SHA-256 `f4abcb6507fced3a6ece90902f5b33bd6c24b07ba38210adc4b9c08f404904ba`;
  full-run monitor `artifacts/logs/monitor-static-des-interface-fix-20261018.log`;
  isolated monitor `artifacts/logs/monitor-des-m1-reset-isolated-20261018.log`.

## 2026-10-18 — Match ROM and imported CryptoException views

- Step/experiment: `jcalgtest-cryptoexception-rom-alias`
- Change: In exception-table matching, recognize the ROM and imported
  `CryptoException` class records (offsets 234 and 233) as the same Java Card
  type. In the terminal status-word path, recognize the ROM `ISOException`
  record at offset 350 alongside the JC305 export view at offset 316.
- Validation: ARM, RISC-V, and PC build wrappers passed. The prior isolated
  physical trace confirmed that after the CryptoException alias match, the
  applet catch ran, recorded reason 3, and threw `ISOException(9000)`; it still
  returned `6F00` because the old terminal ISOException cast failed. The latest
  image reported `FLASH:PASS` but did not re-enumerate as USB serial or CCID,
  so the current fix has not been verified on hardware. The PC VPCD harness
  installed and selected JCAlgTest but timed out at GETVERSION after 20 seconds
  before the isolated APDU.
- Result: The physical failure is an API class-view alias problem through both
  the applet catch and ISO status conversion. The new source covers the two
  observed pairs, but success remains unproven. The current RP2350 device is
  absent from this VM; the authorized PC/SC helper found no Pico.
- Decision: Resume physical checks once the device enumerates. First require
  M1 prepare response `03 9000` and a normal follow-up SELECT, then test a
  bounded static subset and the full matrix. Do not implement the DES padding
  algorithm; preserve `NO_SUCH_ALGORITHM` semantics.
- Commit: `f10a7d6`; metadata commit: `2931ce4`
- Artifacts/logs: build records
  `artifacts/logs/build-arm-catch-alias-final-20261018.log`,
  `artifacts/logs/build-riscv-catch-alias-final-20261018.log`,
  `artifacts/logs/build-pc-catch-alias-final-20261018.log`; physical traces
  `artifacts/logs/monitor-des-m1-alias-fix-20261018.log` and
  `artifacts/logs/monitor-des-m1-iso-class-check-20261018.log`; latest cycle
  `artifacts/logs/cycle-choupi-20261018T131939Z.log`; VPCD timeout
  `artifacts/logs/vpcd-des-m1-iso-alias-20261018.log`.

## 2026-10-18 — Validate exception aliases and isolate static reader loss

- Step/experiment: `physical-jcalgtest-exception-alias-validation`
- Change: Validated the committed ROM/imported `CryptoException` and
  `ISOException` alias handling on the physical Pico 2. Added physical APDU,
  static-client, and focused DES3 workload logs; updated status and recurring
  issue notes.
- Validation: ARM and RISC-V builds passed. `./tools/cycle choupi` passed and
  captured runtime `TEST:SUMMARY pass=1 fail=0`. The exact M1 prepare returned
  `03 9000`, followed by successful SELECT. Official `ALG_PERFORMANCE_STATIC`
  reached M1 and recorded `NO_SUCH_ALGORITHM`, then exited after 382 CSV rows
  with `SCARD_E_READER_UNAVAILABLE` during 3-key DES CBC NOPAD. USB remained
  enumerated; rerunning the authorized PC/SC ACL helper restored the reader.
  The exact 3-key DES CBC NOPAD prepare and 50-call operation then returned
  `AA9000`, with a successful follow-up SELECT. No full static completion is
  claimed.
- Result: The exception-alias fix is physically verified for the previously
  failing unsupported M1 path. The full matrix remains pending due to a
  transient PC/SC reader loss of unknown cause; the affected DES3 operation
  itself passes in isolation. This is not evidence for implementing M1.
- Decision: Diagnose the long-run PC/SC availability drop and rerun the full
  static suite. Then resume the JCSystem/context API audit.
- Commit: `5ff5a32`.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261018T171028Z.log`;
  `artifacts/logs/physical-m1-exception-alias-20261018.log` SHA-256
  `88966d8b0f6844b9b0b4dc323383af73ad6dbbc86af5f02e4cc9be869cdc557e`;
  `artifacts/logs/jcalgtest-static-exception-alias-20261018.log`;
  `artifacts/logs/physical-des3-static-probe-20261018.log`;
  partial CSV and client log under
  `artifacts/results/physical-perf-static-exception-alias-20261018/`.

## 2026-10-18 — Unwind frames after interpreter exception returns

- Step/experiment: `interpreter-stack-unwind-before-heap-sweep`
- Change: Added `Stack::unwind()` and use it for status-word, Java exception,
  timeout, entry, and abort returns from `Interpretor::run()`. This makes the
  after-APDU heap sweep see an empty Java stack after interpretation has ended,
  while preserving root results on normal completion.
- Validation: PC, ARM, and RISC-V builds passed; final CHOUPI cycle passed with
  `TEST:SUMMARY pass=1 fail=0`. On hardware, 50 M1 prepares each returned
  `03 9000`, followed by 3-key DES CBC NOPAD prepare/50-call measurement
  (`AA9000`) and SELECT; all passed. The concurrent serial log contained no
  `HEAP_GC_SKIP`. The official static run reached 512 CSV rows before
  `malloc 3244 failed to allocate memory` and panic during AES CBC PKCS5
  prepare; it is incomplete. The PC/VPCD harness still timed out at GETVERSION.
- Result: Stale frames caused the collector to skip after exception returns;
  the stack unwind removes that blocker. The next failure is collector scratch
  memory exhaustion while growing its traversal worklist, not a missing AES
  algorithm implementation.
- Decision: Bound `traceAndMark()` worklist allocation by total stable handle
  count, then rerun a serial-captured physical static suite. Keep
  `NO_SUCH_ALGORITHM` rows expected.
- Commit: `90238e3`.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261018T173718Z.log`;
  `artifacts/logs/physical-heap-unwind-stress-20261018.log` SHA-256
  `89f3f5cdc99bfb5dec9e63312785d1b689af7d68ca45e799a9c2519834abbf5d`;
  `artifacts/logs/monitor-heap-unwind-stress-20261018.log`;
  `artifacts/logs/jcalgtest-static-heap-unwind-final-20261018.log`;
  `artifacts/logs/monitor-static-heap-unwind-final-20261018.log`;
  partial CSV and client log under
  `artifacts/results/physical-perf-static-heap-unwind-final-20261018/`.

## 2026-10-18 — Stream collector roots and children

- Step/experiment: `heap-gc-stream-root-and-child-visitors`
- Change: Removed dynamic root, pending-worklist, and child-reference buffers
  from the physical collector path. Root and child references are visited
  directly; stable-slot mark bytes track unreachable, pending, and scanned
  states. Updated STATUS, QKB, and the session handoff with observed limits.
- Validation: `./tools/build arm`, `./tools/build riscv`, and `./tools/build-pc`
  passed. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed with
  `CYCLE:PASS` and `TEST:SUMMARY pass=1 fail=0`. Physical JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` reached 544 CSV rows; no collector allocation
  failure, panic, or GC skip appeared in serial output. The client stopped on
  `SCARD_E_READER_UNAVAILABLE` during AES-192. A focused recovery probe once
  passed SELECT, GETVERSION, JCSystem info, and DES prepare (`AA9000`); later
  retries showed intermittent PC/SC reader enumeration/transport failures.
  The static matrix is incomplete. Existing packed CAP-member warnings remain
  in the PC build.
- Result: Streaming roots was needed in addition to bitmap traversal: prior
  attempts failed at 3,244-byte/1,642-byte worklist allocations and a 2,048-byte
  root-vector allocation. The final collector passed the prior failure point
  without panic through 544 rows. Remaining end-to-end blocker is intermittent
  host USB/PC/SC access; `NO_SUCH_ALGORITHM` remains an expected applet result.
- Decision: Diagnose host passthrough/reader instability at the exact failed
  APDU, then rerun the full physical static matrix. Resume the JCSystem/context
  API audit after PC/SC remains stable through the long test.
- Commit: `677d4e3`.
- Artifacts/logs: ARM/RISC-V/PC builds and physical cycle under
  `artifacts/logs/*gc-stream-roots-final-20261018.log`; static client and serial
  logs `artifacts/logs/jcalgtest-static-gc-stream-roots-20261018.log` and
  `artifacts/logs/monitor-static-gc-stream-roots-20261018.log`; partial output
  under `artifacts/results/physical-perf-static-gc-stream-roots-20261018/`;
  PC/SC recovery traces under `artifacts/logs/*gc-stream-roots-recovery*20261018.log`.

## 2026-10-18 — Diagnose PC/SC stall after static test

- Step/experiment: `physical-usb-pcsc-layer-isolation`
- Change: Recorded the observed difference between USB, PC/SC daemon, and Java
  reader enumeration in QKB, STATUS, and the session handoff.
- Validation: The Pico remained visible in `lsusb` as `2e8a:0009`; `pcscd` was
  active, and the authorized helper returned `PCSC:ACCESS_GRANTED`. A bounded
  `timeout 5s pcsc_scan -r` timed out without output. Earlier serial capture
  showed SELECT completed (`9000`) while the following GETVERSION was absent.
  No source or firmware test was run in this diagnostic-only step.
- Result: The collector run itself reached 544 CSV rows with no OOM or panic;
  current PC/SC enumeration is stalled even though USB is present.
- Decision: Request operator restart of `pcscd.service`, because repository
  `AGENTS.md` permits autonomous sudo only for the Pico ACL helper; then
  refresh ACL and check the reader before another physical run.
- Commit: `7b1d519`.
- Artifacts/logs: `artifacts/logs/pcsc-host-state-gc-stream-roots-20261018.log`.

## 2026-10-18 — Isolate PC/SC failure after applet selection

- Step/experiment: `physical-pcsc-select-followup-isolation`
- Change: Updated STATUS, QKB, and HANDOFF with a host-vs-firmware boundary
  diagnosis. The serial capture shows the Pico answered SELECT (`9000`) and
  received no GETVERSION request afterward.
- Validation: The authorized ACL helper succeeded and USB remained visible as
  `2e8a:0009`; `pcscd.service` was active. A post-restart reader scan briefly
  listed the physical Pico and both Virtual PCD readers. The focused
  `probe-jc-algtest --des-cipher-prepare` timed out after successful SELECT;
  a concurrent 18-second serial capture recorded GetSlotStatus and one SELECT
  XfrBlock (`xfr=1`), with runtime self-checks passing and no `FAULT`. `./tools/doctor`
  passed. No source change was made, so builds and firmware tests were
  intentionally skipped.
- Result: The current failure occurs before the host sends GETVERSION; the
  available evidence does not implicate JCVM execution or the collector. The
  physical static run remains incomplete.
- Decision: Continue investigating PC/SC host transport without unbounded
  `pcsc_scan`; keep software/runtime work moving independently.
- Commit: `69c180d`.
- Artifacts/logs: `artifacts/logs/monitor-pcsc-probe-concurrent-20261018.log`
  and `artifacts/logs/monitor-pcsc-after-restart-20261018.log`.

## 2026-10-18 — Check VPCD bridge as a physical PC/SC fallback

- Step/experiment: `physical-ccid-vpcd-bridge-availability`
- Change: Documented the VPCD fallback's runtime requirements and USB ownership
  conflict in STATUS, QKB, and HANDOFF.
- Validation: The installed bridge first connected to the Pico USB interface
  but port 35963 refused the connection. A bounded Java SmartcardIO query
  showed `Virtual PCD 00 00/01` with no card and activated listeners on
  35963/35964. Retrying the bridge then failed at CCID interface claim with
  libusb status `-6` (busy). No firmware or card data was changed.
- Result: The bridge cannot run while the system PC/SC daemon owns the physical
  Pico interface. The PC/SC/reader configuration must release or exclude that
  interface while keeping the virtual reader endpoint available.
- Decision: Do not run the direct bridge concurrently with the PC/SC physical
  reader driver. Continue host configuration recovery before physical JCAlgTest.
- Commit: `84713c0`.
- Artifacts/logs: No persistent bridge log; command output is recorded in this
  session. Existing serial evidence remains in
  `artifacts/logs/monitor-pcsc-probe-concurrent-20261018.log`.
## 2026-10-18 — Verify applet select and deselect callbacks

- Step/experiment: `wp5-applet-reselect-callbacks`
- Change: Added VPCD probe case 55, which records callback counters before and
  after same-AID reselect and asserts exactly one `deselect()` and one `select()`.
- Validation: `./tools/build-jcsystem-transaction-probe` passed. The focused
  `./tools/test-jcsystem-transaction-applet --case 55` passed with counts
  changing from `01 00` to `02 01` (`TEST:SUMMARY pass=3 fail=0`, 13.627 s).
  An initial sandboxed emulator attempt failed with `EPERM`; the host-access
  rerun passed. The preceding `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`
  passed ARM/RISC-V builds, ARM flash, and finite serial runtime checks. The
  physical reader probe then returned SELECT `9000` but timed out before
  GETVERSION, so no physical callback assertion was made.
- Result: The VPCD callback bridge invokes both callbacks once on same-AID
  reselect. Physical consecutive-APDU validation remains blocked by PC/SC.
- Decision: Keep broadening the lifecycle regression on VPCD; do not repeat
  physical APDUs until PC/SC transmits beyond SELECT.
- Commit: `654f922`
- Artifacts/logs: `artifacts/logs/jcsystem-applet-callback-vpcd-20261018.log`,
  `artifacts/logs/cycle-choupi-20261018T194931Z.log`
## 2026-10-18 — Verify rejected applet selection cleanup

- Step/experiment: `wp5-applet-select-rejection`
- Change: Added a test-only flag to reject the next `Applet.select()` callback
  and a VPCD regression for rejection status, process gating, and later
  successful selection.
- Validation: `./tools/build-jcsystem-transaction-probe` passed. Combined
  cases 55/56 passed in one VPCD session: callback counts advanced from
  `01 00` to `02 01`; rejected SELECT returned `6999`; a process APDU while
  unselected returned `6999`; a later SELECT returned `9000`.
  `TEST:SUMMARY pass=4 fail=0 elapsed_s=19.811`.
- Result: The VPCD selection lifecycle clears selected state after a rejected
  callback and permits a subsequent successful selection.
- Decision: Continue the applet lifecycle audit with deselect, reset, and
  persistence boundaries; physical transport remains stalled after SELECT.
- Commit: `c949b1d`
- Artifacts/logs: `artifacts/logs/jcsystem-applet-lifecycle-vpcd-20261018.log`
## 2026-10-18 — Verify applet lifecycle callbacks across card reset

- Step/experiment: `wp5-applet-reset-callbacks`
- Change: Added a VPCD case that compares applet callback counters across card
  reset and reselect.
- Validation: `./tools/test-jcsystem-transaction-applet --case 57` passed.
  Cases 55/56/57 also passed together: ordinary reselect increments both
  callbacks once, rejected selection returns `6999` and blocks process APDUs
  until a successful SELECT, and card reset increments deselect/select once.
  Combined result: `TEST:SUMMARY pass=5 fail=0 elapsed_s=21.503`.
- Result: The VPCD runtime invokes `deselect()` before reset cleanup and invokes
  `select()` on the subsequent successful SELECT.
- Decision: Continue checking lifecycle edge cases and persistent state on
  VPCD; no physical APDU was attempted because PC/SC remains stalled after
  SELECT.
- Commit: `ee50fa0`
- Artifacts/logs: `artifacts/logs/jcsystem-applet-lifecycle-callbacks-vpcd-20261018.log`

## 2026-10-19 — Verify persistent applet state across emulator restart

- Step/experiment: `wp5-applet-persistent-restart`
- Change: Added `--restore-existing` to the JCSystem VPCD runner, which reuses
  saved memory and restores the installed applet without reinstalling. The
  runner now closes VPCD and waits for the emulator's normal save path before
  terminating it. Added case 58 for the committed instance value.
- Validation: Both emulator processes used the same 512 KiB memory image. The
  first committed field value `0x33`; a fresh process restored it and passed
  static reference identity, persistent array child identity, object-array
  alias/mutation, restored array checkcast, and field-value checks:
  `TEST:SUMMARY pass=7 fail=0 elapsed_s=9.520`. An initial attempt used case 7,
  which correctly expected the separate abort value `0x11`; after observing
  `0x33`, the dedicated case 58 passed with the restore checks.
- Result: Committed instance data and persistent reference relationships
  survived a full PC emulator process restart.
- Decision: Continue GP DELETE/GET DATA and transaction failure/power-loss
  validation on VPCD. The physical reader still stalls after SELECT.
- Commit: `fc05835`
- Artifacts/logs: `artifacts/logs/jcsystem-restore-save-vpcd-20261019.log`,
  `artifacts/logs/jcsystem-restore-second-process-vpcd-20261019.log`,
  `artifacts/logs/cycle-choupi-20261019T035222Z.log`

## 2026-10-19 — Confirm transaction failure and power-cut recovery coverage

- Step/experiment: `continue-storage-recovery-audit`
- Change: Reconciled `STATUS.md` and `TODO.md` with existing storage-level rollback and power-cut coverage; retained the open end-to-end GP DELETE and card power-cycle checks.
- Validation: `./tools/test-persistent-transaction` passed all 29 checks; `./tools/test-rp2350-fs-overlay` passed all 27 checks, including 1,000 randomized multi-record power-cut cases. Existing upstream compiler warnings and host `mallinfo()` deprecation warning remain.
- Result: Injected record-write and snapshot-commit failures roll back cleanly in the transaction test; randomized filesystem recovery returns an atomic old or new multi-record state. No physical hardware was accessed.
- Decision: Continue with authenticated card-manager DELETE integration and post-delete restore/GET STATUS; do not repeat already-covered storage-layer tests without a new scenario.
- Commit: `2a793fe`; this commit records the validated step
- Artifacts/logs: tool output from this session; no separate log captured.
## 2026-10-19 — Implement GlobalPlatform Card Data GET DATA

- Step/experiment: `gp-get-data-card-recognition`
- Change: Added Card Recognition Data (`0066`) encoding for the supported GlobalPlatform recognition OID and SCP03 S8. Wired it into open-channel discovery and authenticated CCID card-manager GET DATA dispatch. CPLC remains unavailable (`6A88`) because no manufacturing-data source exists.
- Validation: `./tools/test-gp-get-data` passed 11/11 exact-encoding, response-status, and unavailable-tag checks; `./tools/build arm` and `./tools/build riscv` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed both build gates, flashed ARM, and captured repeated `TEST:PASS choupi_runtime` in 18 seconds with no `FAULT`. No card-manager APDU was sent.
- Result: The firmware now returns the bounded `0066` data object before and after secure-channel authentication and retains `6A88` for CPLC/unknown tags. Firmware boot/runtime is hardware-verified; GET DATA APDU transport is unverified.
- Decision: Continue with authenticated DELETE lifecycle regression; verify GET DATA through a live APDU path when available.
- Commit: `2ccb6e0`; recorded in follow-up metadata commit
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261019T061401Z.log`; test output from this session.
## 2026-10-19 — Verify GlobalPlatform GET DATA on physical Pico

- Step/experiment: `gp-get-data-physical-apdu`
- Change: Added a bounded PC/SC probe for open-channel Card Recognition Data and unavailable CPLC, with exact TLV/status assertions and no applet selection.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/probe-gp-get-data` passed on the Pico. `80CA006600` returned `6618731606072A864886FC6B01640B06092A864886FC6B0403109000`; `80CA9F7F00` returned `6A88`. Reader ATR was `3B800181`.
- Result: The open-channel GET DATA path works over physical CCID/PCSC. The authenticated channel route was not exercised.
- Decision: Continue with authenticated GP DELETE lifecycle coverage and test authenticated GET DATA separately.
- Commit: `0ca6a57`; recorded in follow-up metadata commit
- Artifacts/logs: `artifacts/logs/gp-get-data-physical-20261019.log`

## 2026-10-19 — Verify SCP03-authenticated GET DATA on physical Pico

- Step/experiment: `gp-get-data-scp03-physical`
- Change: Extended the bounded PC/SC probe to derive SCP03 session keys, verify the card cryptogram, authenticate with C-MAC, and send protected GET DATA.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/probe-gp-get-data` passed on the named Pico reader. INITIALIZE UPDATE and EXTERNAL AUTHENTICATE both returned `9000`; protected `84CA006608CB721BF61EAE254100` returned the expected Card Recognition Data and `9000`. `git diff --check` passed; firmware builds were not repeated because only probe/docs changed.
- Result: Open-channel and SCP03-authenticated GET DATA both work physically. The probe did not select an applet or modify persistent card content.
- Decision: Proceed to authenticated GP DELETE lifecycle coverage, including GET STATUS and restore.
- Commit: `43a0721`
- Artifacts/logs: `artifacts/logs/gp-get-data-physical-20261019.log`

## 2026-10-19 — Cover GET STATUS after GP deletion states

- Step/experiment: `gp-get-status-delete-state-views`
- Change: Added exact registry-response checks for an instance-deleted/package-present view and an empty package registry after package deletion.
- Validation: `./tools/test-gp-registry` passed 47 checks; targeted `git diff --check` passed. The test exercises GET STATUS encoding only, not persistent deletion.
- Result: The registry builder emits no applet after instance deletion while preserving the package entry; a package-deleted view emits no load-file entry.
- Decision: Keep actual authenticated DELETE dispatch, `CapLoadSession::deleteObject()`, and restart restoration as open work; next add a testable persistence/CAP-store boundary.
- Commit: `8b9095d`; validated step, recorded in metadata commit `4fb7515`
- Artifacts/logs: test output from this session; no separate log captured.

## 2026-10-19 — Exercise GP DELETE transaction sequencing offline

- Step/experiment: `gp-delete-transaction-backend`
- Change: Extracted the persistent transaction sequence from `CapLoadSession::deleteObject()` into a small templated backend routine; the firmware adapter still calls the existing flash/filesystem operations. Added fake-backend tests for success and each rollback/error branch.
- Validation: `./tools/test-gp-delete` passed 21/21 checks. `./tools/build arm` passed, including the CHOUPI ARM image; `./tools/build riscv` passed. `git diff --check` passed. No hardware cycle was run because the Pico is in BOOTSEL.
- Result: The same transaction ordering production uses is now host-testable. Tests cover begin, applet/package clearing, tombstone write, snapshot sync/commit, rollback, and marker exceptions. They do not exercise actual flash records, CAP-slot deletion, authenticated CCID dispatch, or restart restoration.
- Decision: Continue toward a storage-backed `CapLoadSession` test boundary and retain the physical DELETE/restart lifecycle as open.
- Commit: `2536480`; recorded in follow-up metadata commit
- Artifacts/logs: test and build output from this session; no separate build logs captured.

## 2026-10-19 — Test GP DELETE target resolution offline

- Step/experiment: `gp-delete-target-resolution`
- Change: Extracted package/instance AID resolution and package dependency checks into `gp::resolve_delete_target()` and routed `CapLoadSession::deleteObject()` through the shared decision code.
- Validation: `./tools/test-gp-delete` passed 26/26 checks. `./tools/build arm` and `./tools/build riscv` passed. `git diff --check` passed. No hardware cycle was run because the Pico is in BOOTSEL.
- Result: The production resolver returns `6A88` for unknown/unloaded targets, `6985` for package deletion with an installed instance and no related-object flag, and selects valid instance/package targets without mutating storage.
- Decision: Next exercise persistent lifecycle markers, GET STATUS, CAP deletion, and restore through a storage-backed host session; authenticated CCID DELETE remains unverified.
- Commit: `108ac39`; recorded in follow-up metadata commit
- Artifacts/logs: test and build output from this session; no separate logs captured.

## 2026-10-19 — Verify GP DELETE and persistence on the Pico

- Step/experiment: `gp-delete-pico-restart-persistence`
- Change: Added `tools/test-pico-powerup`, a bounded post-boot check that verifies the JCAlgTest package/applet registry and applet SELECT, GETVERSION, and JCSystem calls. Added a `--version-only` mode to the existing hardware APDU probe.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed ARM/RISC-V build gates, reflashed ARM, and captured runtime passes. GPPro authenticated applet DELETE left the package loaded; authenticated related-object package DELETE removed both; reinstall of the supplied CAP succeeded. After a second CHOUPI firmware restart, GPPro listed the package as `LOADED` and applet as `SELECTABLE`; `tools/test-pico-powerup` passed, including SELECT `9000`, GETVERSION `1.8.2_jc305`, and JCSystem `9000`. The full post-restart `tools/probe-jc-algtest` also passed SHA-256 repeated and static calls. `bash -n tools/test-pico-powerup` passed.
- Result: Authenticated instance/package deletion, CAP reinstall, and persistence through firmware restart/reflash are verified on the real Pico. One prior `INSTALL` watchdog marker was reported once during boot; runtime self-checks passed and no `FAULT` occurred.
- Decision: The environment cannot perform true USB power removal; `picotool reboot` failed because the composite device has no accessible reset interface. Keep physical power-off/on validation open and run `tools/test-pico-powerup` after such a cycle when available.
- Commit: `eb700d2`; QKB addendum `aa99182`
- Artifacts/logs: `artifacts/logs/gp-delete-physical-20261019.log`, `artifacts/logs/cycle-choupi-20261019T065052Z.log`, `artifacts/logs/pico-powerup-persistence-20261019T065455Z.log`

## 2026-10-19 — Retry Pico power-up PC/SC checks

- Step/experiment: `pico-powerup-pcsc-retry`
- Change: Bounded the registry and applet APDU phases of `tools/test-pico-powerup` to three attempts by default, refreshing the authorized USB ACL before retries. Added configurable positive timeouts and recorded current PC/SC failure evidence in TODO, STATUS, HANDOFF, and QKB.
- Validation: `bash -n tools/test-pico-powerup` and targeted `git diff --check` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` exited 1 after all three GPPro attempts listed only Virtual PCD readers. `PICO2_HOST_USB_ACCESS=1 lsusb -d 2e8a:0009` found the Pico; bounded `PICO2_HOST_USB_ACCESS=1 pcsc_scan -r` listed only the two virtual readers. No ARM build or flash was needed because firmware sources were unchanged.
- Result: The failure is now bounded and logged per attempt. USB enumeration and ACL access do not currently restore Pico PC/SC enumeration, so applet persistence was not tested in this run.
- Decision: Keep physical persistence and consecutive APDUs open. Continue offline implementation; retry after the PC/SC reader is available.
- Commit: `73eaca3`; progress record follows in metadata commit.
- Artifacts/logs: `artifacts/logs/pico-powerup-persistence-20261019T084640Z.log`, `artifacts/logs/pico-powerup-persistence-20261019T085227Z.log`

## 2026-10-19 — Implement APDU current object access

- Step/experiment: `apdu-get-current-apdu`
- Change: Implemented `APDU.getCurrentAPDU()` with a native bridge to the active context's APDU object. The VM scopes access to `Applet.process()` and raises Java `SecurityException` from `Applet.select()`. Added VPCD regression cases for object identity and access rejection.
- Validation: `./tools/build arm` passed for ARM M33; API converter reported zero errors (existing optional export/int-support warnings and compiler packed-member warnings remain). `./tools/build-jcsystem-transaction-probe` passed. `./tools/test-jcsystem-transaction-applet --case 59 --case 60 --timeout 120 --log artifacts/logs/jcsystem-current-apdu-vpcd-20261019.log` passed (`TEST:SUMMARY pass=4 fail=0 elapsed_s=11.280`). No RISC-V build or physical flash was attempted. The physical reader was not available through PC/SC.
- Result: During `process()`, `getCurrentAPDU()` returned the same reference as the callback argument; during `select()`, it produced the expected `SecurityException`.
- Decision: Continue the Java Card APDU API/profile audit and add further focused golden coverage; retain physical validation as open until PC/SC enumerates the Pico.
- Commit: `2780db1`; Java API hunk committed in nested API repository as `43f9877`.
- Artifacts/logs: `artifacts/logs/jcsystem-current-apdu-vpcd-20261019.log`

## 2026-10-19 — Gate APDU current buffer access

- Step/experiment: `apdu-get-current-apdu-buffer`
- Change: Implemented `APDU.getCurrentAPDUBuffer()` by returning the active
  APDU object's buffer, so the existing process-scope guard also protects the
  buffer API. Added VPCD cases for buffer identity and rejection during
  `Applet.select()`; updated status, handoff, and PC/SC issue notes.
- Validation: `./tools/build arm` and `./tools/build-jcsystem-transaction-probe`
  passed. VPCD cases 59–62 passed (`TEST:SUMMARY pass=6 fail=0
  elapsed_s=33.705`). The requested `PICO2_HOST_USB_ACCESS=1
  ./tools/test-pico-powerup` acquired USB ACL access but all three GPPro
  registry attempts timed out before applet APDUs. No RISC-V build or flash was
  attempted. Targeted `git diff --check` passed.
- Result: During `Applet.process()`, the method returned the exact same buffer
  as the active APDU object's `getBuffer()`; during `Applet.select()`, it
  raised `SecurityException`. Physical persistence remains inconclusive due
  to missing Pico PC/SC registry visibility.
- Decision: Keep physical power-up validation open and continue the APDU API
  audit offline until the Pico reader enumerates through PC/SC.
- Commit: root `433d5c2`; nested API repository `2ec9efa`.
- Artifacts/logs: `artifacts/logs/jcsystem-current-apdu-buffer-vpcd-20261019.log`,
  `artifacts/logs/pico-powerup-persistence-20261019T092025Z.log`

## 2026-10-19 — Implement APDU wait-extension state checks

- Step/experiment: `apdu-wait-extension-state`
- Change: Replaced the `APDU.waitExtension()` stub with its APDU error-state and
  no-chaining checks. Added an internal native query for the current command's
  no-chaining flag and VPCD cases for both rejected and accepted modes.
- Validation: `./tools/build arm` passed, including Java Card export/ROM-mask
  generation and the RP2350 ARM image. `./tools/build-jcsystem-transaction-probe`
  passed. VPCD cases 63–64 passed (`TEST:SUMMARY pass=4 fail=0
  elapsed_s=14.110`). No RISC-V build or flash was attempted; no physical
  serial validation was run. Targeted staged and unstaged `git diff --check`
  passed.
- Result: `waitExtension()` returns in ordinary outgoing mode and throws
  `APDUException.ILLEGAL_USE` after `setOutgoingNoChaining()`. The CCID stack
  still has no asynchronous T=1 WTX path, so transport-level wait-time extension
  is not implemented.
- Decision: Continue auditing APDU and JCSystem runtime semantics; revisit T=1
  WTX if the target profile requires long-running applet timeout extension.
- Commit: root `122e01e`; nested API repository `edfa4cf`.
- Artifacts/logs: `artifacts/logs/jcsystem-apdu-wait-extension-vpcd-20261019.log`

## 2026-10-19 — Implement JCSystem.lookupAID for the active applet

- Step/experiment: `jcsystem-lookup-aid-active-context`
- Change: Implemented `JCSystem.lookupAID()` null/range validation and active-AID matching; added VPCD cases 65–66. Fixed JC305 AID method-token translation (`{0, 4, 2, 1, 3, 5}`), which had dispatched JC305 `getBytes()` as the embedded four-argument `partialEquals()` method and raised `StackUnderflowException`.
- Validation: `./tools/build arm` passed. `./tools/build-jcsystem-transaction-probe` passed. `./tools/build-pc` passed and refreshed the emulator. VPCD cases 65–66 passed (`TEST:SUMMARY pass=4 fail=0 elapsed_s=24.373`); the current AID matched, an unknown AID returned null, and all null/range checks passed. The default restricted VPCD invocation returned `Operation not permitted`; the same local VPCD command in the host execution context passed. No RISC-V build or hardware flash was run. The requested `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` received USB ACL access but all three GPPro attempts listed only Virtual PCD readers; persistence was inconclusive. `git diff --check` passed with `core.whitespace=cr-at-eol` for the existing CRLF sources.
- Result: The call path works for the single active applet context. Lookup among multiple installed applets is still limited by the VM's single-context registry model. The initial failed VPCD retry was against a stale host emulator; rebuilding PC/VPCD is required for trustworthy validation.
- Decision: Keep the one-context limitation explicit; continue the Java Card API/profile audit and retry physical persistence only after the Pico reader enumerates in PC/SC.
- Commit: API `aa4c7e0`; root `5f995a0`; progress/docs/QKB recorded in follow-up metadata commit.
- Artifacts/logs: `artifacts/logs/jcsystem-lookup-aid-fixed-vpcd-20261019.log` (SHA-256 `00c28d3a4f316c9c69f9d202f149fa41dc70e09b544034c5f56c8156dd7a72ba`); diagnostic successful run `artifacts/logs/jcsystem-lookup-aid-diagnostic-vpcd-20261019.log` (SHA-256 `e43cc19da4c91ea176fd9ab14f10695feb61dd2034c6cec407484d43e7a40a39`); physical reader discovery retry `artifacts/logs/pico-powerup-persistence-20261019T095458Z.log` (SHA-256 `4d6bd6d6f532c82dda5aa93f28163d14d546bd79fc513af2929a300aadf5f9fa`).

## 2026-10-19 — Retry Pico power-up persistence check

- Step/experiment: `pico-powerup-persistence-retry-102428Z`
- Change: Re-ran the repository power-up test after the user reinserted the Pico; archived its bounded discovery log.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` exited 1 after three registry attempts. USB access was granted for `/dev/bus/usb/001/004`, but GPPro listed only `Virtual PCD 00 00` and `Virtual PCD 00 01`; a subsequent escalated `pcsc_scan -r` also listed only those virtual readers. No package/APDU persistence assertion ran. No build or flash was attempted.
- Result: Physical persistence remains unverified because PC/SC does not currently expose the Pico reader to the client.
- Decision: Keep hardware power-up validation open; retry after Pico registration appears in `pcsc_scan -r`.
- Commit: `8eda0c2`.
- Artifacts/logs: `artifacts/logs/pico-powerup-persistence-20261019T102428Z.log` (SHA-256 `4d6bd6d6f532c82dda5aa93f28163d14d546bd79fc513af2929a300aadf5f9fa`)


## 2026-10-19 — Implement Applet.reSelectingApplet lifecycle state

- Step/experiment: `applet-reselecting-state`
- Change: Track same-AID reselection across card-manager, VPCD, and applet lifecycle paths. `Applet.reSelectingApplet()` now queries a scoped VM flag during `Applet.select()`; rejected selections, reset, and power-off clear the flag. Added regression assertions for true/false states.
- Validation: `./tools/build arm` passed. The focused VPCD lifecycle cases passed, then the full `tools/tests/jcsystem_transaction_applet_test.py --timeout 240` suite passed (`TEST:SUMMARY pass=69 fail=0 elapsed_s=442.556`). It verified the flag is true only for same-AID reselection and false after rejected selection and reset. No RISC-V build, physical flash, or hardware runtime test was performed.
- Result: Same-AID reselection semantics and the surrounding callback ordering pass the complete current VPCD JCSystem/APDU regression.
- Decision: Continue the Java Card runtime API/profile audit; physical power-cycle persistence remains open because the latest GPPro/PCSC registry only exposed Virtual PCD readers.
- Commit: API `814d409`; root `c098a86`.
- Artifacts/logs: `artifacts/logs/applet-reselecting-vpcd-20261019.log` (SHA-256 `02a733eb484772581e4e65390f0abdc4e3ad8754bcf91230da6a62f57db08950`); `artifacts/logs/jcsystem-full-reselect-vpcd-20261019.log` (SHA-256 `bb139accf35ce574e0b49ac0facee2483c7c35bf6e8437f554fc74d673a34ad4`).

## 2026-10-19 — Implement Applet.register installation semantics

- Step/experiment: `applet-register-installation-context`
- Change: Replaced bytecode-token recognition with explicit native API calls.
  Both overloads now require the install entry point and reject duplicate
  registration. The explicit AID overload checks length, bounds, and equality
  with the install instance AID. The probe records caught install-time failure
  reasons and falls back to no-argument registration for mismatch testing.
- Validation: `./tools/build-pc`,
  `./tools/build-jcsystem-transaction-probe`, and `./tools/build arm` passed.
  VPCD cases 67–70 passed for the matching install AID
  (`TEST:SUMMARY pass=6 fail=0 elapsed_s=16.536`). A mismatched AID produced
  `SystemException.ILLEGAL_AID` reason `04`, then fallback registration passed
  (`TEST:SUMMARY pass=3 fail=0 elapsed_s=11.097`). No RISC-V build or physical
  flash was performed. The PC build emitted legacy converter/Rust warnings;
  the ARM build completed with `BUILD:PASS arch=arm` and
  `BUILD:PASS target=choupi`.
- Result: Install-time registration state, matching/mismatched AID handling,
  and post-install rejection are verified on VPCD. Hardware persistence of a
  newly installed applet remains unverified because PC/SC currently omits the
  physically USB-enumerated Pico.
- Decision: Continue the lifecycle audit from `Applet.selectingApplet()` and
  its ordering relative to applet SELECT/PROCESS callbacks.
- Commit: API `5cd9aec`; root source/tests `3b9e6de`.
- Artifacts/logs: ARM build `artifacts/logs/build-arm-applet-register-final-20261019.log`
  (SHA-256 `e68ffebbbed5a0d78478656e18834f004007ec0d0a41dbc57c757fa575efb12a`);
  matching install `artifacts/logs/applet-register-validation-vpcd-20261019-retry2.log`
  (SHA-256 `a2c7e8f6229ce52840bf9ef63aa85be9eee32c3a8c917a1008b7b16f6214fb00`);
  mismatched install `artifacts/logs/applet-register-mismatched-aid-vpcd-20261019-retry.log`
  (SHA-256 `14ea7fac44ebc178cbcfd043b1a60fe417c8fb14f9ac5e59347b8d6a25e32eea`).

## 2026-10-19 — Implement selectingApplet lifecycle and correct CAP Method bounds

- Step/experiment: `applet-selecting-cap-method-bounds`
- Change: `Applet.selectingApplet()` now reflects execution during `select()` and
  the subsequent SELECT APDU `process()` call. The CCID and VPCD paths deliver
  the original SELECT APDU to the applet after a successful selection. Fixed
  CAP Method component length decoding to convert network byte order and remove
  both the handler-count byte and exception-handler table from the method-code
  array length.
- Validation: `CHOUPI_JCVM_DEBUG=OFF ./tools/build-pc`,
  `./tools/build-jcsystem-transaction-probe`, and `./tools/build arm` passed.
  `./tools/test-vpcd-session` passed (`TEST:SUMMARY pass=15 fail=0`). The full
  VPCD applet regression passed 75 checks in 276.309 s. No RISC-V build,
  physical flash, or applet APDU was attempted. The separate power-up probe
  refreshed USB access, but GPPro listed only Virtual PCD readers through all
  three retries and exited before applet APDUs.
- Result: Lifecycle state was observed as true during select and SELECT
  processing, false during deselect, and correct across reset, rejected select,
  and reselect. The 4,102-byte CAP Method component now installs correctly;
  it previously appeared as 1,552 bytes and failed at method offset 4,089.
- Decision: Continue the Java Card API/runtime audit; keep physical power-up
  persistence open because PC/SC currently exposes only Virtual PCD readers.
- Commit: root `694b408`; API `bf323aa`.
- Artifacts/logs: `artifacts/logs/applet-lifecycle-full-vpcd-final-20261019.log`,
  `artifacts/logs/test-vpcd-session-selecting-applet-20261019.log`,
  `artifacts/logs/build-arm-method-component-endian-fix-20261019.log`, and
  diagnostic `artifacts/logs/applet-selecting-debug-vpcd-20261019.log`.

## 2026-10-19 — Retry Pico power-up persistence after reinsertion

- Step/experiment: `pico-powerup-persistence-retry-105115Z`
- Change: Re-ran the bounded physical power-up persistence check after the user
  reinserted the Pico; archived the reader-discovery log.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` exited 1
  after three attempts. The authorized USB ACL helper granted access to
  `/dev/bus/usb/001/004`, and `lsusb -d 2e8a:0009` confirmed the Pico. GPPro
  listed only `Virtual PCD 00 00` and `Virtual PCD 00 01` on each attempt;
  escalated `pcsc_scan -r` confirmed the same reader list. The persistence test
  did not reach package verification or applet APDUs. No build or flash was
  attempted.
- Result: The physical Pico is USB-enumerated but absent from PC/SC. Power-up
  persistence remains unverified.
- Decision: Resume after `pcscd.service` has been restarted interactively,
  refresh access with the authorized helper, and confirm the named Pico reader
  using a bounded scan before rerunning the test.
- Commit: pending metadata commit.
- Artifacts/logs: `artifacts/logs/pico-powerup-persistence-20261019T105115Z.log`
  (SHA-256 `4d6bd6d6f532c82dda5aa93f28163d14d546bd79fc513af2929a300aadf5f9fa`).

## 2026-10-19 — Reject unknown applet SELECT AIDs

- Step/experiment: `unknown-aid-select-routing`
- Change: Added short SELECT-by-AID recognition to CCID and VPCD routing. A
  valid unknown AID now deselects the active applet and returns `6A82` instead
  of being passed to `Applet.process()`. Added a unit regression and VPCD case
  74; the unit test also covers a nonzero optional short Le byte.
- Validation: `./tools/test-vpcd-session` passed (16/16). The focused VPCD applet
  test passed (3/3): unknown SELECT returned `6A82`, and a subsequent applet
  command returned `6999`. `CHOUPI_JCVM_DEBUG=OFF ./tools/build-pc`,
  `./tools/build-jcsystem-transaction-probe`, and `./tools/build arm` passed.
  PC Rust dependency compilation emitted its existing 49 warnings. RISC-V
  build and physical flash/APDU validation were intentionally skipped.
- Result: Both transport paths reject unknown short AID SELECTs without
  dispatching to the selected applet. The physical reader remains absent from
  PC/SC, so no on-device result is available.
- Decision: Continue the Java Card runtime/API audit in TODO.md; retry physical
  persistence when PC/SC lists the Pico reader.
- Commit: `d9ea607`.
- Artifacts/logs: `artifacts/logs/test-vpcd-session-unknown-aid-select-20261019.log`,
  `artifacts/logs/unknown-aid-select-vpcd-20261019.log`,
  `artifacts/logs/build-pc-unknown-aid-select-20261019.log`, and
  `artifacts/logs/build-arm-unknown-aid-select-20261019.log`.

## 2026-10-19 — Automate VPCD applet persistence restart check

- Step/experiment: `vpcd-applet-persistent-field-process-restart`
- Change: Added a selectable flash-image path to the JCSystem applet VPCD test
  runner and `tools/test-vpcd-persistence`. The wrapper builds the PC emulator
  and probe, commits persistent instance field `0x33`, then starts a fresh
  emulator process with the same saved flash image and verifies the restored
  value. The image is isolated in a temporary directory.
- Validation: `PICO2_VPCD_PERSISTENCE_TIMEOUT=180 ./tools/test-vpcd-persistence`
  passed. The first process committed
  `0x33` (`339000`) in 11.997 s; the fresh process restored and returned
  `0x33` (`339000`) in 7.159 s. PC emulator, Java Card API/ROM mask, and probe
  builds passed. The legacy converter emitted Maven repository warnings, but
  the build completed successfully. No ARM build or physical flash was needed
  for this host-only check; physical power-off/on remains unverified.
- Result: Registered applet instance state survives a clean host emulator
  process restart and decode from the saved filesystem image.
- Decision: Keep physical AC5 open until the Pico is visible through PC/SC and
  a true power-off/on persistence check can run.
- Commit: `1d30dd9`.
- Artifacts/logs: build log SHA-256
  `bc8d84e356f0509f9434fcebae5e6a5cd781296b12a567783b85c0051da249df`;
  commit-phase output `artifacts/logs/vpcd-persistence-commit-20261019T120347Z.log`;
  restore-phase output `artifacts/logs/vpcd-persistence-restore-20261019T120347Z.log`;
  emulator logs `artifacts/logs/vpcd-persistence-install-emulator-20261019T120347Z.log`
  and `artifacts/logs/vpcd-persistence-restore-emulator-20261019T120347Z.log`.

## 2026-10-19 — Check restored persistent object graph

- Step/experiment: `vpcd-persistent-reference-graph-restart`
- Change: Expanded `tools/test-vpcd-persistence` so its restore process also
  checks persistent static reference identity, reference-array child identity,
  shared array aliases and mutation, and restored array type, in addition to
  the registered applet's committed field.
- Validation: `PICO2_VPCD_PERSISTENCE_TIMEOUT=180 ./tools/test-vpcd-persistence`
  passed. Install/commit phase passed 3/3; fresh restore phase passed 7/7. The
  restored value was `0x33`; all five restore APDUs returned their expected
  data and `9000`. PC emulator and probe builds passed, with 49 existing Rust
  dependency warnings. No target build or physical hardware was required for
  this host-only persistence test.
- Result: The process-restart check now covers the applet instance and the
  persistent reference graph. It does not prove physical flash persistence
  across a real power-off/on.
- Decision: Keep physical AC5 open; continue the AC4 and Java Card runtime
  audit while the Pico is absent from PC/SC.
- Commit: `636fbfc`.
- Artifacts/logs: build log SHA-256
  `4b945b5f65a409f02c30458f0023a2c8a75c9642333b57d1eb487b4e8d8c04c9`;
  restore transcript SHA-256
  `bd596435e2c7678cf88cdac881147a39049062281b85b2494796268a2bf4d82b`;
  `artifacts/logs/vpcd-persistence-commit-20261019T121017Z.log`,
  `artifacts/logs/vpcd-persistence-install-emulator-20261019T121017Z.log`,
  and `artifacts/logs/vpcd-persistence-restore-emulator-20261019T121017Z.log`.

## 2026-10-19 — Validate JCAlgTest support comparison

- Step/experiment: `jcalgtest-support-declaration-comparison`
- Change: Added a reusable v1.8.2 CSV comparator and a reviewed declaration of
  the 15 CHOUPI-supported result identities. Updated the AC4 report and TODO
  to distinguish expected simulator capability differences from stale
  physical evidence.
- Validation: `python3 -m py_compile tools/compare-jcalgtest-support.py`
  passed. Comparing the archived Pico and jCardSim CSVs matched all 4,525
  `(section, algorithm_name)` rows; CHOUPI had 15 `yes`, 4,510 `no`, zero
  other statuses, and exact agreement with all 15 declared supported rows.
  `./tools/build arm` passed. The current-source VPCD persistence check passed
  install/commit 3/3 and restore 7/7. RISC-V build was intentionally skipped
  per the ARM-only project scope.
- Result: Archived AC4 inputs are structurally consistent and CHOUPI support
  statuses match the declaration. The physical CSV predates the OwnerPINX
  implementation, so AC4 remains open pending a fresh physical run.
- Decision: Continue runtime implementation and VPCD checks; retry hardware
  support and power-cycle tests when PC/SC exposes the Pico CCID reader.
- Commit: `4863b56`.
- Artifacts/logs: comparator output SHA-256
  `945b10b7ea4c29aa50805bc94a4f88caf8169f6bb3de5f518d033cbc2fe1b882`
  (`artifacts/logs/jcalgtest-support-compare-20261019.log`); latest power-up
  attempt SHA-256
  `4d6bd6d6f532c82dda5aa93f28163d14d546bd79fc513af2929a300aadf5f9fa`
  (`artifacts/logs/pico-powerup-persistence-20261019T121536Z.log`).

## 2026-10-19 — Make AID byte comparisons null-safe

- Step/experiment: `aid-null-safe-array-comparisons`
- Change: Updated `AID.equals(byte[], short, byte)` and
  `AID.partialEquals(byte[], short, byte)` to return false for null arrays,
  matching the Java Card 3.0.5 contract. Added applet case 75 to exercise null,
  matching full-AID, and matching prefix comparisons.
- Validation: `./tools/build-pc` regenerated the Java Card API and ROM mask and
  built the PC emulator successfully; existing converter/Maven warnings and a
  blocked hostname lookup were reported, without build errors. The focused
  VPCD applet test passed 3/3 (`TEST:SUMMARY pass=3 fail=0 elapsed_s=15.877`).
  `./tools/build arm` passed for `rp2350-arm-s`. No RISC-V build or physical
  flash was performed.
- Result: Null AID comparisons are catch-free false results, while valid full
  and prefix comparisons remain true in the applet VM.
- Decision: Continue the JCSystem and runtime API audit; physical validation
  waits for PC/SC to expose the Pico reader.
- Commit: API `f3e41e0`; applet regression and TODO `c9fc0eb`.
- Artifacts/logs: `artifacts/logs/aid-null-comparison-20261019.log`, SHA-256
  `eb6bc6f6ac662f62b69e36b1ab2b8708e29792003df006ad74b441c50e266541`.

## 2026-10-19 — Prepare functionality review implementation guide

- Step/experiment: `functionality-review-handoff`
- Change: Added `review_20261019.md` with code locations, proposed fixes,
  regressions, and completion criteria for three functionality issues, plus a
  deferred SCP03 key-provisioning note. Updated the session handoff.
- Validation: Read-only inspection of the cited code paths and test wrappers;
  documentation diff check passed. Builds and hardware checks were intentionally
  skipped because no source code changed.
- Result: Follow-up agents have separable implementation steps and validation
  gates. No runtime metric was measured and none of the proposed fixes was
  implemented or validated in this step.
- Decision: Start with distinct instance-AID restoration, then SELECT `61xx`;
  design CAP/filesystem recovery separately and verify injected power cuts.
- Commit: this documentation commit (`git log -1 --format=%H -- review_20261019.md`).
- Artifacts/logs: `review_20261019.md`; no runtime logs.
## 2026-10-19 — Allow distinct applet instance AIDs during restore

- Step/experiment: `distinct-applet-instance-aid-restore`
- Change: Removed the CAP applet-AID equality check from installed-instance
  restoration. A temporary PC VPCD CLI experiment used a distinct instance AID
  without retaining edits to the already-dirty emulator source.
- Validation: The temporary distinct-AID probe passed install/commit 3/3 and
  fresh-process restore 7/7. The retained `./tools/test-vpcd-persistence`
  passed install/commit 3/3 and restore 7/7. `./tools/build arm` passed for
  RP2350 ARM M33. No flash was attempted; physical reboot testing is pending.
- Result: The temporary probe used CAP Applet AID `4A43416C675465737431` and
  instance AID `A00000015150455253495354`. It selected the instance and restored
  persisted objects and field `0x33`. The PC emulator restore path does not
  invoke firmware-only `CapLoadSession::restoreInstalled()`, so that exact
  metadata reader is compile-checked but not directly exercised.
- Decision: Keep the AID distinction in GlobalPlatform metadata and retry
  hardware power-cycle validation when Pico PC/SC is available.
- Commit: `b3da80e`
- Artifacts/logs: temporary distinct-AID logs
  `artifacts/logs/vpcd-persistence-commit-20261019T181446Z.log`,
  SHA-256 `d8fc996e3cbd758be9e62dc7b7b1af006bc7b27ae81ea0d65ca66f5077d35d95`;
  `artifacts/logs/vpcd-persistence-restore-20261019T181446Z.log`, SHA-256
  `7b898e84a3a5bfa6c878739dced131933b1cfa136d5f73e76e496fb12b878bc3`; final
  retained regression logs `artifacts/logs/vpcd-persistence-commit-20261019T181900Z.log`,
  SHA-256 `8982ccad9a7fba7a96510069b9dfcd3a67ce91e83de955962da98bec6a94b9a5`;
  `artifacts/logs/vpcd-persistence-restore-20261019T181900Z.log`, SHA-256
  `482f43b2eff35de0ee80df934efc65837d72ea59384b3b3b2c64ff5c0a6b7f4a`.

## 2026-10-19 — Preserve applet selection for SELECT 61xx

- Step/experiment: `select-get-response-continuation`
- Change: Added shared SELECT status handling for `9000` and `61xx`. CCID and
  VPCD no longer deselect the applet when SELECT reports pending response data.
  Added VPCD coverage proving GET RESPONSE is dispatched while the applet stays
  selected.
- Validation: `./tools/test-vpcd-session` passed 18 checks. `./tools/build arm`
  passed for RP2350 ARM M33. No firmware flash or physical APDU was attempted.
- Result: A `6103` SELECT result leaves the VPCD applet selected and the next
  `00 C0 00 00 03` APDU reaches its process callback. Non-`9000`/non-`61xx`
  selection failures still clear selection.
- Decision: Keep the explicit status rule in both transports; continue the
  card-manager behavior audit.
- Commit: `86ffde9`
- Artifacts/logs: Test summary `TEST:SUMMARY pass=18 fail=0`; no archived log.

## 2026-10-19 — Record CAP install cross-store recovery risk

- Step/experiment: `cap-install-cross-store-commit-review`
- Change: Documented the mismatch between the CAP slot's installed header and
  the VM filesystem transaction, and made fault injection/reconciliation an
  explicit open persistence item.
- Validation: Read-only inspection of `CapLoadSession::installAndMakeSelectable`,
  `cap_store_mark_installed`, and
  `FlashMemory_Handler::commitPersistentWriteTransaction`; bounded
  `timeout 8s pcsc_scan -r` listed only the two Virtual PCD readers. No injected
  sync failure, flash, or physical APDU was attempted.
- Result: The CAP metadata write occurs before the final `fs_sync()`. A failed
  sync rolls back filesystem snapshots, while the CAP header is stored outside
  those snapshots. This is a source-confirmed failure window, not a reproduced
  card failure.
- Decision: Add failure injection for the final sync and CAP metadata write
  before implementing a durable intent/reconciliation design.
- Commit: `cc292ef`
- Artifacts/logs: No artifact log; source references are recorded in
  `.agent/QKB.md`.

## 2026-10-19 — Reject incomplete applet restore metadata

- Step/experiment: `restore-requires-class-index`
- Change: Firmware restore now leaves an applet uninstalled when the CAP slot
  does not contain a valid persisted applet class index. This prevents
  incomplete or legacy metadata from advertising an applet that cannot be
  reconstructed.
- Validation: `./tools/build arm` passed. `./tools/test-vpcd-persistence`
  passed install/commit (3/3) and fresh-process restore (7/7). No RISC-V build
  or flash was run under the ARM-only scope.
- Result: ARM CHOUPI image ends at `0x100bb520`. On hardware, GPPro confirmed
  the package/applet registry and SELECT returned `9000`; GETVERSION failed
  with `SCARD_E_NOT_TRANSACTED`. The Pico remained visible to `lsusb` and serial
  reported `TEST:PASS choupi_runtime`, but PC/SC lost its reader. Physical
  applet processing and power-cycle persistence remain unverified.
- Decision: Keep the restore guard. Diagnose post-SELECT PC/SC/CCID loss
  separately; continue offline applet/runtime work.
- Commit: `169c131`
- Artifacts/logs: `artifacts/logs/vpcd-persistence-commit-20261019T182907Z.log`
  (SHA-256 `f5eac0e3cbecfff2ab93bdaaecb770190cd10b06fc6e20e8e7f57ca8c895b49d`),
  `artifacts/logs/vpcd-persistence-restore-20261019T182907Z.log` (SHA-256
  `c24e0c55c89c4ea759f5e667f50b4e792f51187ec5e2feb7804e5fcb9ab89a58`), and
  `artifacts/logs/pico-powerup-persistence-20261019T183156Z.log` (SHA-256
  `8e39987aea3e657be75315a089103a79ac0785a7e713facc392c5dca89d73e38`).

## 2026-10-19 — Track applet activity for JCSystem.isAppletActive

- Step/experiment: `jcsystem-is-applet-active-lifecycle`
- Change: Added active lifecycle state to `Context`. The VM marks the context
  active after a successful applet `select()` and clears it after `deselect()`
  and card reset. `JCSystem.isAppletActive(AID)` now also requires that state,
  avoiding a false positive during install. Added probe case 76.
- Validation: `./tools/build-pc` passed (Java API/ROM-mask and host VM); focused
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet --case 76`
  passed 3/3 in 14.986 s. `./tools/build riscv` passed; `PICO2_HOST_USB_ACCESS=1
  ./tools/cycle arm` passed the ARM smoke check, and `PICO2_HOST_USB_ACCESS=1
  ./tools/cycle choupi` passed ARM/RISC-V build gates, flash, and the 18-second
  CHOUPI runtime capture. No `TEST:FAIL` or `FAULT` appeared.
- Result: Case 76 confirmed `isAppletActive` false during install and true after
  SELECT. Physical follow-up listed the Pico and read ATR `3B800181`, but timed
  out before SELECT response; serial counters remained `stage=2`, `rx_events=0`,
  `tasks=0`, `xfr=0`. No physical applet APDU reached firmware.
- Decision: Keep the lifecycle state fix. Continue JCSystem/runtime work on
  VPCD; diagnose PC/SC transport before relying on physical APDU tests.
- Commit: `da6a6e6`
- Artifacts/logs: `artifacts/logs/jcsystem-is-applet-active-lifecycle-20261019.log`
  (SHA-256 `2785080c8ae40cbc52f38cd02b278ff398bb9b9de36837ff2ba73d6b102138b1`),
  `artifacts/logs/cycle-arm-20261019T184719Z.log` (SHA-256
  `ee199babc6c764a3a746ad975ec3a931d06374cd4c60e697fd80b106068d4426`),
  `artifacts/logs/cycle-choupi-20261019T184813Z.log` (SHA-256
  `63fc54ebb7072a42ed270283acf8f377ca43c7383d1b7391c66d9874ddaeb8fe`), and
  `artifacts/logs/physical-jcsystem-active-retry-20261019.log` (SHA-256
  `3aa9ca87751a04bc1308d72070d03648593f460ddc367996a9d9f5a677d5d3fa`).

## 2026-10-19 — Cover active state inside applet callbacks

- Step/experiment: `jcsystem-is-applet-active-callbacks`
- Change: Extended the applet probe to inspect `JCSystem.isAppletActive()` from
  `select()` and `deselect()`, alongside the existing install/process checks.
  The test drives ISD SELECT to invoke deselect, then selects and processes the
  probe applet.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet
  --case 77` passed 3/3 in 23.691 s. Responses confirmed ISD SELECT `9000`, applet
  SELECT `9000`, and lifecycle mask `0F9000`.
- Result: `isAppletActive` is false during install and select callbacks, true
  during deselect and process callbacks. No firmware source changed in this
  validation-only step, so builds and flash were not repeated.
- Decision: Keep the active-state timing and retain callback assertions in the
  focused lifecycle test.
- Commit: `e1ebcfe`
- Artifacts/logs: `artifacts/logs/jcsystem-is-applet-active-callbacks-20261019.log`
  (SHA-256 `ae2eedbd46cbe0e25c2995dc958109caa36d69d47404ffaeb45b182002567705`).

## 2026-10-19 — Retry Pico firmware and applet power-up probe

- Step/experiment: `pico-powerup-apdu-transport-retry`
- Change: Flashed the reinserted BOOTSEL Pico with the current CHOUPI ARM image
  and recorded the physical PC/SC/APDU failure in `.agent/QKB.md` for future
  diagnosis.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` reached and passed
  its ARM/RISC-V build gates and recorded `FLASH:PASS`. The cycle log stopped
  before serial capture, so `PICO2_HOST_USB_ACCESS=1 ./tools/monitor --seconds
  18` was run separately and showed repeated runtime pass summaries. The
  authorized PC/SC helper and `timeout 8s pcsc_scan -r` listed the Pico. A
  single-attempt `tools/test-pico-powerup` confirmed the JCAlgTest package and
  selectable applet, read ATR `3B800181`, then timed out transmitting SELECT.
  Follow-up finite serial capture reported `rx_events=0`, `tasks=0`, `xfr=0`.
- Result: Firmware flashed and booted; physical applet execution and true
  power-cycle persistence remain unverified because no XfrBlock reached the
  firmware. No source-code tests were run in this hardware-only step.
- Decision: Do not repeat the same probe until the host PC/SC/CCID transmit path
  is diagnosed. Continue implementation and VPCD verification offline.
- Commit: `0c31204d8468adea9ffc669d9c38aa09518b13f5`
- Artifacts/logs: `artifacts/logs/pico-powerup-persistence-20261019T191503Z.log`
  (SHA-256 `14ecbe9209f62be8fd7b3f3327ef553e54d2d1fb52202bf808ad57086cfb5ed4`),
  `artifacts/logs/monitor-apdu-probe-after-timeout-20261019.log` (SHA-256
  `3dc13d33a620ae7cc5dd1ccfa38029139d678e2682fbff2de4c0516ea2a8dad8`), and
  `artifacts/logs/cycle-choupi-20261019T191306Z.log`.

## 2026-10-19 — Correct applet-active state during callbacks

- Step/experiment: `jcsystem-is-applet-active-callback-contract`
- Change: Matched `JCSystem.isAppletActive(AID)` to the Java Card 3.0.5
  contract. It now returns true after successful registration while `install()`
  runs, and the context becomes active before `select()` is invoked. The active
  flag is cleared when `select()` fails or rejects selection and after
  `deselect()` completes. Updated lifecycle regressions to expect true in the
  documented callbacks.
- Validation: Focused VPCD cases 76, 77, and 56 passed (`3/3` each; 14.540 s,
  22.576 s, and 28.979 s). `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`
  passed both build gates, flashed the ARM image, and passed the 18-second
  runtime capture with no `TEST:FAIL` or `FAULT`.
- Result: The implementation follows the JC 3.0.5 callback behavior and keeps
  the flag cleared after a rejected select. The Java physical-card probe remains
  blocked before the first SELECT XfrBlock by the PC/SC/CCID transport issue.
- Decision: Keep the corrected callback semantics; use VPCD for applet behavior
  until consecutive physical APDUs reach the firmware.
- Commit: `f6b5439209eabf47824cd8bbc6a50933dd6c178c`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261019T192429Z.log` (SHA-256
  `8518fcfe2c8ae4369e41a4fdd2bd931505ff866031cd321555ff100ab99a1e4a`), plus
  `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261016.log` emitted by the
  focused applet VPCD regression.

## 2026-10-19 — Return null from JCSystem.getAID before registration

- Step/experiment: `jcsystem-get-aid-registration-state`
- Change: `NativeImplementation.getAID()` now returns a null reference until
  `Applet.register()` succeeds or the VM restores a previously installed
  applet. The focused probe records the `getAID()` result before registration
  and asserts that it is null.
- Validation: The first VPCD attempt used a stale PC emulator and failed this
  new assertion; after `./tools/build-pc`, the focused install test passed
  (`TEST:SUMMARY pass=3 fail=0`, 17.964 s). A fresh-process restore run also
  passed (`pass=3 fail=0`, 9.986 s). `./tools/cycle choupi` passed ARM/RISC-V
  build gates, flashed the updated ARM image, and passed the 18-second runtime
  check. Existing Cargo/C++ warnings remain in the dependency/build output.
- Result: The Java-level `getAID()` value now follows the registration state
  before install, after install, and after emulator process restore.
- Decision: Keep the registration guard and include null-before-register in
  JCSystem lifecycle regressions.
- Commit: `15dc3fcc5c5ead672e52d1ed5634ec78459e457d`
- Artifacts/logs: `artifacts/logs/jcsystem-get-aid-registration-vpcd-20261019.log`,
  `artifacts/logs/jcsystem-get-aid-registration-restore-vpcd-20261019.log`, and
  `artifacts/logs/cycle-choupi-20261019T193502Z.log`.

## 2026-10-19 — Synchronize JCSystem status documentation

- Step/experiment: `jcsystem-status-contract-refresh`
- Change: Updated `STATUS.md` and `TODO.md` to reflect the JC 3.0.5
  `isAppletActive()` callback contract and the new null-before-register
  `getAID()` behavior, including VPCD evidence and the outstanding physical
  APDU transport limitation.
- Validation: Documentation inspection only; no build, test, or hardware
  command was required for this metadata update.
- Result: The project status and TODO no longer describe the previous incorrect
  install/select behavior or claim Java-level `getAID()` coverage is absent.
- Decision: Keep the status files synchronized with committed behavior and
  evidence.
- Commit: `14a8edccfb7518458f9074017ed9b9db14b5828d`
- Artifacts/logs: `STATUS.md`, `TODO.md`.



## 2026-10-19 — Translate transient allocation failure to Java Card exception

- Step/experiment: `transient-array-no-space-exception`
- Change: Removed `noexcept` from the allocating `JCVMArray` constructor so
  allocation errors propagate. Primitive and reference transient-array factories
  now map `std::bad_alloc` to `SystemException.NO_TRANSIENT_SPACE` (reason 2);
  invalid transient clear events set `SystemException.ILLEGAL_VALUE` (reason 1).
- Validation: `./tools/build arm` passed. The restricted VPCD invocation returned
  `EPERM`; rerunning through the repository's required host-access path passed
  cases 28 and 29 (`TEST:SUMMARY pass=4 fail=0`, 39.084 s). No flash was run.
- Result: Transient array clear-event behavior remains correct and allocation
  failure can now reach Java Card exception dispatch rather than terminating in
  the backing-array constructor. Deterministic exhaustion injection was not run.
- Decision: Keep the exception translation; add allocation-failure injection
  before claiming the low-memory path fully verified.
- Commit: `da50522af16913d8b7dc1351c805b7ed15b1d66e`
- Artifacts/logs: `artifacts/logs/jcsystem-transient-allocation-errors-vpcd-20261019.log`.


## 2026-10-19 — Inject transient backing-array allocation failure

- Step/experiment: `jcvmarray-allocation-failure-regression`
- Change: Added a standalone focused test that overrides array allocation to
  fail deterministically and checks the JCVM backing-array constructor propagates
  `std::bad_alloc`. Added `tools/test-jcvmarray-allocation` as its repository
  wrapper.
- Validation: `./tools/test-jcvmarray-allocation` passed with
  `TEST:PASS allocation_failure_propagates`.
- Result: Confirms the constructor no longer terminates when backing allocation
  fails. The full VM's transient-factory reason-2 mapping remains inspection-only.
- Decision: Keep the regression and continue the API/runtime audit; add full-VM
  low-memory injection if a safe allocator seam is introduced.
- Commit: `61cdcdedf3068dd8126b8b2da54c2b958ce8121a`
- Artifacts/logs: none.


## 2026-10-19 — Propagate persistent array restoration allocation failures

- Step/experiment: `persistent-array-restore-allocation-propagation`
- Change: Removed `noexcept` from the persistent `JC_Array` constructor, which
  allocates its tagged backing buffer. Extended the allocation regression with a
  static assertion that the constructor is permitted to propagate failure.
- Validation: `./tools/build arm` passed. The first test build failed because
  `-Wpedantic -Werror` promoted legacy flexible-array and qualifier warnings in
  included CAP headers; the wrapper was narrowed to `-Wall -Wextra` with the two
  existing header warning classes suppressed. The final
  `./tools/test-jcvmarray-allocation` passed (`TEST:PASS
  allocation_failure_propagates`).
- Result: Allocation failure from persistent-array construction can now unwind
  to restore/APDU handling instead of terminating at the constructor boundary.
  Its exact initializer failure was not injected.
- Decision: Keep the change; add a constructor-level fault injection seam only if
  needed for broader restore low-memory coverage.
- Commit: `a64a9d72b7387c4a15797930dd638b418762daab`
- Artifacts/logs: none.


## 2026-10-19 — Inject failure in persistent array constructor

- Step/experiment: `persistent-array-allocation-failure-injection`
- Change: Extended the focused allocation regression to construct a persistent
  `JC_Array` with a real `Heap` and inject `std::bad_alloc` into its tagged
  backing-buffer `new[]` initializer. Added the required OS FFI include path and
  `jref_t.cpp` test linkage.
- Validation: `./tools/test-jcvmarray-allocation` passed both
  `allocation_failure_propagates` and
  `persistent_array_failure_propagates`. Its first integrated build lacked the
  FFI include path; the next link lacked `jref_t.cpp`; the corrected wrapper
  completed cleanly. The ARM build passed after the source change in the preceding
  step.
- Result: Both the general JCVM array constructor and the persistent array
  constructor propagate deterministic allocation failures. Full VM restore/APDU
  handling remains unverified under forced exhaustion.
- Decision: Keep the targeted regression; continue the broader runtime/API audit.
- Commit: `292802da16f02c12915b87381eb0a18d52bb8301`
- Artifacts/logs: none.


## 2026-10-19 — Release temporary buffers after persistent field reads

- Step/experiment: persistent-field-read-buffer-ownership
- Change: getDataFromTag() now returns std::unique_ptr<uint8_t[]>; byte,
  short, and int field getters release the temporary buffer after use or during
  exception unwinding. Added allocation/deallocation tracking to the persistent
  transaction test, including an injected filesystem read error.
- Validation: ./tools/test-persistent-transaction passed with
  TEST:SUMMARY fail=0; tracked successful and failed reads each allocated and
  freed exactly one array. ./tools/build arm passed. The first test run printed
  existing warnings from CAP headers; its wrapper now suppresses the existing
  qualifier and packed-member warning classes and the final run was clean.
- Result: Persistent primitive field reads no longer retain temporary buffers
  in the runtime heap, including on read failures.
- Decision: Keep the ownership change and allocation-count regression; continue
  the runtime memory audit.
- Commit: `01f7eba75fc1afc53dbabcecb68cf4cdf5e307ed`
- Artifacts/logs: none.


## 2026-10-19 — Verify persistent-read ownership through applet restore

- Step/experiment: persistent-field-read-vpcd-restore
- Change: Ran the full two-process VPCD persistence workflow after the owned
  persistent-field read buffer change.
- Validation: PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence passed.
  The install/commit process passed 3/3 checks and fresh-process restore passed
  7/7, including static reference identity, persistent array-child identity,
  object-array alias identity/mutation, restored array type, and committed field
  0x33. ARM source build had already passed for the implementation commit.
  The host rebuild emitted existing Java 1.7, converter, Maven, and packed
  CAP-header warnings; the run completed successfully.
- Result: The ownership change remains compatible with persistent applet restore
  and object graph reconstruction in the host VPCD emulator.
- Decision: Keep the change; physical persistence still requires a real board
  power cycle and functioning PC/SC applet APDUs.
- Commit: `71c6811c33e15f3672d31b2fb37c2efcea123785`
- Artifacts/logs: artifacts/logs/vpcd-persistence-commit-20261019T203104Z.log
  and artifacts/logs/vpcd-persistence-restore-20261019T203104Z.log.


## 2026-10-19 — Keep persistent instance construction exception-safe

- Step/experiment: persistent-instance-construction-raii
- Change: The persistent JC_Instance constructor now holds the optional overflow
  tag buffer and fields array in local unique_ptrs, transferring ownership into
  the object only after all potentially throwing setup completes.
- Validation: ./tools/build arm passed. PICO2_HOST_USB_ACCESS=1
  ./tools/test-vpcd-persistence passed install/commit 3/3 and fresh-process
  restore 7/7. The constructor's own failure path was not directly injected.
  Existing Rust/C++ and packed CAP-header warnings appeared during the host
  rebuild; the command completed successfully.
- Result: Failure while resolving fields or allocating the fields array cannot
  leak the persistent tag overflow buffer.
- Decision: Keep the RAII staging; continue the runtime memory/API audit.
- Commit: source `1ac2955fbb901c13158f94481d0bfd1497c9ad92`;
  validation/docs `b2947afa6cdb494dcdc17b758df567ae9d0d5f00`.
- Artifacts/logs: artifacts/logs/vpcd-persistence-commit-20261019T203708Z.log
  and artifacts/logs/vpcd-persistence-restore-20261019T203708Z.log.


## 2026-10-19 — Preserve JCVMArray when copy allocation fails

- Step/experiment: jcvmarray-copy-allocation-safety
- Change: JCVMArray copy construction now stages an owned copy until element
  copying finishes. Mutable copy assignment stages its replacement before
  releasing the destination's old backing buffer.
- Validation: ./tools/test-jcvmarray-allocation passed
  allocation_failure_propagates, copy_assignment_preserves_destination, and
  persistent_array_failure_propagates. ./tools/build arm passed.
- Result: A failed mutable array copy no longer leaves a nonzero length with a
  null backing pointer; assignment failure preserves the destination's values.
- Decision: Keep the staged ownership and continue the memory/API audit.
- Commit: `79c2a79c0fd42e21f154bf5e973bca9e7f497d83`
- Artifacts/logs: none.


## 2026-10-19 — Exercise JCVMArray copy-constructor allocation failure

- Step/experiment: jcvmarray-copy-construction-allocation-failure
- Change: Extended the focused allocation regression to inject `std::bad_alloc`
  while copying a mutable `JCVMArray` and assert that the failure propagates.
  The copied array's size and first element are observed on the success path so
  the host compiler cannot elide the unused copy and its allocation.
- Validation: `./tools/test-jcvmarray-allocation` passed all four checks:
  `allocation_failure_propagates`, `copy_construction_failure_propagates`,
  `copy_assignment_preserves_destination`, and
  `persistent_array_failure_propagates`. The first run exposed the test's
  unused-copy optimization and failed as expected; the observable-use revision
  passed. ARM build was not rerun because only the host regression changed.
- Result: Copy-constructor failure propagation is now deterministically covered
  alongside the existing construction, assignment, and persistent-array cases.
- Decision: Keep the focused test and continue the runtime memory/API audit.
- Commit: `b8545a9`
- Artifacts/logs: none.


## 2026-10-20 — Reserve runtime exception root before heap mutation

- Step/experiment: runtime-exception-root-allocation-order
- Change: `Heap::addRuntimeException()` reserves one root-vector slot before
  allocating and inserting the corresponding Java exception object. If root
  table growth fails, no unrooted object has been added to the heap.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/build-pc`
  passed and regenerated the ROM mask. Existing converter, Maven/network, and
  packed-CAP warnings appeared in the host build. The bounded VPCD integration
  connected but stopped during applet restore without an APDU result; runtime
  exception behavior was not verified. No hardware operation was run.
- Result: Root-vector growth now happens before the heap object allocation.
  Direct failure injection at the reserve boundary remains outstanding.
- Decision: Keep the ordering fix; continue the heap allocation audit.
- Commit: `f311958`
- Artifacts/logs: `artifacts/logs/runtime-exception-root-reserve-vpcd-20261020.log`
  (inconclusive VPCD emulator log).


## 2026-10-20 — Reserve static GC roots before persistent reference writes

- Step/experiment: static-reference-root-reserve-before-persist
- Change: Added `Heap::reserveStaticReferenceRoot()` and call it in
  `Static_Handler::setPersistentRef()` before writing the persistent reference.
  Root-vector allocation failure now occurs before the persistent store changes;
  the final root assignment uses reserved capacity.
- Validation: `./tools/build arm` and `./tools/build riscv` passed.
  `./tools/test-persistent-transaction` passed with `TEST:SUMMARY fail=0`.
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed install/commit
  3/3 and fresh-process restore 7/7, including static reference identity,
  persistent array-child identity and aliases, and committed instance data.
  The host rebuild emitted existing converter/Maven and packed-CAP warnings.
  No hardware flash or physical APDU was run; root-reserve allocation failure
  was not directly injected.
- Result: Persistent reference writes can no longer commit before the heap has
  capacity to retain their GC root.
- Decision: Keep the ordering fix and continue auditing heap root-vector
  mutation paths.
- Commit: `9154aa8`
- Artifacts/logs: `artifacts/logs/vpcd-persistence-commit-20261020T035837Z.log`,
  `artifacts/logs/vpcd-persistence-restore-20261020T035837Z.log`.


## 2026-10-20 — Inject static GC root reservation failure

- Step/experiment: static-reference-root-reservation-failure-injection
- Change: Extended the focused allocation test with scalar-allocation failure
  injection for `Heap::reserveStaticReferenceRoot()`. It checks that failure
  leaves no registered root, a retry can reserve and register the handle, and
  reserving an existing key is allocation-free.
- Validation: `./tools/test-jcvmarray-allocation` passed all nine checks with a
  warning-free build. No firmware build or hardware operation was needed since
  this step changes only the host regression.
- Result: The failure behavior of static-root capacity reservation is now
  deterministic and covered directly.
- Decision: Keep the regression and continue auditing heap root-vector mutation
  paths.
- Commit: `45581f9`
- Artifacts/logs: none.


## 2026-10-20 — Bound runtime-exception allocation test to the lightweight harness

- Step/experiment: runtime-exception-root-test-link-boundary
- Change: Tried to exercise `Heap::addRuntimeException()` allocation failure
  directly from the small allocation-test executable. Its `addInstance()`
  dependency pulls in `JC_Instance`, CAP, filesystem, and generated FFI
  definitions that are intentionally absent from that focused target. Removed
  the un-linkable case and recorded where direct coverage belongs.
- Validation: The attempted link failed with unresolved JC_Instance and
  filesystem/FFI symbols. After removing that attempt,
  `./tools/test-jcvmarray-allocation` passed all nine checks.
- Result: The static-root reservation failure is directly tested; runtime
  exception-root ordering remains build-validated but lacks direct allocator
  injection.
- Decision: Keep the lightweight harness focused; use a dependency-complete heap
  integration target if direct runtime-root failure injection becomes necessary.
- Commit: `7a96432`
- Artifacts/logs: none.


## 2026-10-20 — Flash CHOUPI and retry physical persistence probe

- Step/experiment: `physical-choupi-runtime-and-powerup-check`
- Change: Flashed the freshly built ARM CHOUPI image and ran the bounded
  physical package/applet and APDU persistence probe; updated the hardware
  troubleshooting knowledge base and handoff with the transport result.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed ARM and
  RISC-V builds, flashed successfully after CDC BOOTSEL recovery, and captured
  18 seconds of serial output with repeated runtime `TEST:PASS` and
  `TEST:SUMMARY pass=1 fail=0`. `PICO2_HOST_USB_ACCESS=1
  ./tools/test-pico-powerup` confirmed the package/applet registry but failed
  physical SELECT with `SCARD_E_NOT_TRANSACTED`; the next retry found zero Pico
  devices. A bounded post-failure serial monitor again showed runtime passes
  and `rx_events=0 tasks=0 xfr=0`.
- Result: Firmware is running and self-tests pass; the physical applet APDU did
  not reach the CCID handler, so power-up persistence is not established.
- Decision: Keep the transport issue open; retry physical applet testing after
  Pico USB/PCSC visibility has been restored, checking counters after one APDU.
- Commit: `3b73df7`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261020T041200Z.log`;
  `artifacts/logs/pico-powerup-persistence-20261020T041334Z.log`.


## 2026-10-20 — Repeat BOOTSEL flash and diagnose PC/SC reader loss

- Step/experiment: `physical-choupi-pcsc-reproduction`
- Change: Reflashed the board after BOOTSEL reinsertion, repeated the bounded
  physical persistence probe, and checked USB enumeration, PC/SC discovery,
  CCID descriptors, and firmware receive counters.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed both build
  gates and flashed ARM; its 18-second runtime capture repeatedly passed with
  `TEST:SUMMARY pass=1 fail=0`. `./tools/test-pico-powerup` again confirmed the
  JCAlgTest package/applet but failed SELECT with `SCARD_E_NOT_TRANSACTED`;
  retry found zero Pico devices. `lsusb` still found `2e8a:0009`; the
  authorized ACL helper restored the reader in a bounded `pcsc_scan -r`.
  Follow-up serial capture passed runtime checks with `rx_events=0 tasks=0
  xfr=0`. Descriptor inspection found the CCID interface and 64-byte bulk
  endpoints; `journalctl -u pcscd` returned no visible entries.
- Result: This reproduces a host-side PC/SC transport/reader loss before the
  firmware receives any XfrBlock. Physical applet execution and persistence
  remain unverified.
- Decision: Pause further APDU retries until the host PC/SC transmit or reader
  disconnect path is diagnosed; firmware remains responsive.
- Commit: `f06546a`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261020T041650Z.log`;
  `artifacts/logs/pico-powerup-persistence-20261020T041802Z.log`.


## 2026-10-20 — Trace CCID command history during physical SELECT timeout

- Step/experiment: `physical-ccid-command-history`
- Change: Added CCID unknown-command count and a 12-message rolling command
  history to the existing serial diagnostics. This identifies commands that
  precede PC/SC transmit failures without logging APDU payloads.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed ARM and
  RISC-V builds, flashed ARM after two bounded BOOTSEL attempts, and captured
  repeated runtime `TEST:SUMMARY pass=1 fail=0`. A monitored bounded
  `./tools/probe-jc-algtest --version-only` timed out (exit 124) after ATR.
  Serial showed repeated `0x65` GetSlotStatus commands, then `0x62` IccPowerOn
  and SELECT XfrBlock; the applet lookup, restore, and select completed with
  `9000`. The host did not print an APDU response or GETVERSION. No unknown
  commands or endpoint errors appeared. Subsequent PC/SC scans listed only
  virtual readers or timed out, while USB enumeration still found `2e8a:0009`.
- Result: The SELECT reaches and executes in firmware; response delivery to the
  host is still unverified. The board is USB-visible but not presently listed
  by PC/SC. Physical persistence remains unverified.
- Decision: Keep the command-history diagnostic and resume with a longer
  bounded response-path probe after PC/SC discovery is restored.
- Commit: `<recorded in follow-up progress commit>`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261020T055535Z.log`; the
  concurrent SELECT trace was inspected live and not archived.


## 2026-10-20 — Trace physical GC stall after applet SELECT

- Step/experiment: `physical-apdu-post-select-gc-trace`
- Change: Added direct slot iteration, mark-byte live-slot checks, cached
  persistent array lengths, and serial checkpoints around GC root and pass
  boundaries. The slot access change removes linked-list walks from the object
  scan and every visited edge.
- Validation: ARM and RISC-V builds passed. Four CHOUPI hardware cycles
  passed the runtime check (`TEST:SUMMARY pass=1 fail=0`). The focused physical
  SELECT/version probe repeatedly failed with `SCARD_E_NOT_TRANSACTED` after
  the applet returned `9000`. The latest serial trace found 7 array slots, 6
  instance slots, and 13 pending roots; the first pass scanned all objects,
  then the second pass began without returning from GC. The reader is currently
  visible through `pcsc_scan -r`; USB ID `2e8a:0009` shows firmware running,
  not BOOTSEL mode.
- Result: APDU response delivery is blocked inside or after the second GC pass.
  The complexity improvement did not resolve the observed stall; its root cause
  remains unknown. Both architecture builds and runtime checks pass.
- Decision: Keep instrumentation and inspect per-object child traversal and
  persistent-storage reads before changing GC semantics.
- Commit: `71c164b`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261020T061302Z.log`,
  `061623Z.log`, `061949Z.log`, and `062341Z.log`; APDU serial traces were
  inspected live and not archived.


## 2026-10-20 — Record GC diagnostic commit

- Step/experiment: `physical-apdu-post-select-gc-trace-record`
- Change: Recorded the implementation and physical validation commit in the
  progress log and current handoff.
- Validation: Documentation consistency inspection; no firmware rebuild or
  hardware command was needed for this metadata-only update.
- Result: Commit `71c164b` contains the GC traversal optimization attempt and
  the diagnostic findings; physical APDU delivery remains blocked.
- Decision: Continue with per-object traversal instrumentation and fix the
  observed second-pass stall.
- Commit: `56dca05`
- Artifacts/logs: none.


## 2026-10-20 — Grow GC marks for lazily restored heap slots

- Step/experiment: `gc-mark-bitmap-lazy-restore-growth`
- Change: Resize the corresponding mark bitmap when persistent-reference
  traversal lazily adds heap slots. Each pass uses a snapshot of the current
  slot count so appended objects are scanned on the next pass; a reference to
  a previously empty slot refreshes liveness if lazy restoration reused it.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The
  `./tools/cycle choupi` passed and flashed ARM firmware. The physical probe
  returned SELECT `9000`, GETVERSION `1.8.2_jc305`, and JCSystem info `9000`;
  serial showed GC complete after heap growth from 7 arrays / 6 instances to
  27 / 17. `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` passed registry
  discovery and the SELECT/version/JCSystem probe.
- Result: This resolves the reproduced out-of-bounds mark access during lazy
  heap growth and restores physical APDU response delivery in the focused
  tests. Full static performance testing remains pending.
- Decision: Run official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` with serial
  capture to verify repeated collection under the benchmark workload.
- Commit: `0f60673`
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261020T063106Z.log`;
  `artifacts/logs/pico-powerup-persistence-20261020T063339Z.log`.

## 2026-10-20 — Reclaim unreachable persistent heap objects

- Step/experiment: `gc-unpin-persistent-heap-objects`
- Change: Removed blanket GC roots for every loaded persistent-tagged object; collection now follows current context/runtime roots and reachable fields while leaving persistent flash records available for later lazy restoration. Moved marks to heap objects to avoid bitmap allocations during collection. Added opt-in GC summaries for diagnosis.
- Validation: `./tools/test-vpcd-persistence` passed 3/3; `./tools/build arm` and `./tools/build riscv` passed; physical `./tools/cycle choupi` passed before and after diagnostics were disabled; official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed on the Pico; final named-reader SELECT/GETVERSION/JCSystem probe passed.
- Result: Full static suite ended at 2,262 transfers with no panic, invalid refs, or edge failures. GC stabilized at 83 array slots / 28 instance slots (33 / 25 reachable); one collection reclaimed 50 arrays. Expected client totals: 1,772 `NO_SUCH_ALGORITHM`, 95 `CANT_BE_MEASURED`. SHA-256 256-byte measurements: `doFinal` 3.10 ms/op, `update` 3.34 ms/op, `reset` 6.11 ms/op. Final serial had `TEST:PASS choupi_runtime`; one `WATCHDOG_RECOVERY cause=other` diagnostic is the known retained scratch marker on ordinary reboot.
- Decision: Use reachability-based reclamation for this workload; continue auditing static-root and alias coverage for other applets.
- Commit: `dbbbb13`
- Artifacts/logs: `artifacts/logs/jcalgtest-static-gc-root-fix-20261020.log`; `artifacts/logs/monitor-static-gc-root-fix-20261020.log`; `artifacts/logs/cycle-choupi-20261020T082304Z.log`; `artifacts/logs/cycle-choupi-20261020T085457Z.log`; `artifacts/results/physical-perf-static-gc-root-fix-20261020/`.

## 2026-10-20 — Verify BOOTSEL reinsertion detection

- Step/experiment: `rp2350-bootsel-reinsertion-observation`
- Change: Recorded the current BOOTSEL enumeration and corrected the expected RP2350 BOOTSEL USB product ID in the QKB and handoff.
- Validation: Escalated read-only `lsusb`, `udevadm info`, `lsblk`, `findmnt`, `picotool info -a`, and BOOTSEL volume metadata queries.
- Result: Device enumerated as `2e8a:000f`, picotool reported `boot type: bootsel`, and Linux exposed `/dev/sdb` with serial `D92E743D5F640C0F` and auto-mounted the `RP2350` volume. No additional unplug/replug cycle or latency measurement was performed.
- Decision: Reinsertion is recognized by this VM for the observed event; capture udev remove/add timing during a future directly monitored cycle if timing/repeatability is needed.
- Commit: `56dca05`
- Artifacts/logs: Read-only outputs from the current session; no log file created.

## 2026-10-20 — Consolidate current implementation backlog

- Step/experiment: `jcvm-current-backlog`
- Change: Added prioritized `todo.md` with current implementation gaps,
  remaining acceptance checks, completed evidence, and explicit crypto scope.
  Marked uppercase `TODO.md` as historical and linked the current backlog from
  the session handoff.
- Validation: Cross-checked the backlog against `plan.md`, `STATUS.md`, the
  latest `.agent/HANDOFF.md` entries, `.agent/QKB.md`, and the session digests.
  Documentation consistency inspection only; no build or hardware test was
  needed.
- Result: Current open work distinguishes implementation gaps from required
  end-to-end verification; the completed 2,262-transfer physical static run
  is recorded as passing.
- Decision: Use `todo.md` as the single prioritized backlog; preserve old
  uppercase TODO details as historical context.
- Commit: `ec25917`
- Artifacts/logs: none.

## 2026-10-20 — Consolidate backlog into existing TODO tracker

- Step/experiment: `merge-current-backlog-into-TODO`
- Change: Cross-checked the new prioritized backlog against existing `TODO.md`,
  moved the distinct current items into its leading current-status section,
  labeled older checklist entries as historical, redirected the handoff, and
  removed the duplicate lowercase `todo.md`.
- Validation: Compared each backlog item with existing acceptance, persistence,
  GlobalPlatform, and API entries; checked the edited Markdown diff for
  whitespace issues. Documentation-only update; no build or hardware test was
  needed.
- Result: `TODO.md` is the single tracker and includes the latest passing
  2,262-transfer physical static result plus the remaining acceptance and VM
  gaps.
- Decision: Continue maintaining the current prioritized section at the top
  of `TODO.md`; retain older evidence below it for history.
- Commit: `74ec9c8`
- Artifacts/logs: none.

## 2026-10-20 — Refresh physical JCAlgTest supported-algorithm results

- Step/experiment: `physical-alg-support-ownerpinx-20261020`
- Change: Rebuilt both architectures, flashed current CHOUPI ARM firmware, and
  reran the official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` suite on the named
  physical Pico after implementing OwnerPINX. Updated AC4 status and handoff.
- Validation: `./tools/build arm` and `./tools/build riscv` passed;
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed its ARM/RISC-V gates,
  flash, and 18-second serial check (`TEST:PASS choupi_runtime`,
  `TEST:SUMMARY pass=1 fail=0`, no `FAULT`). Physical PC/SC showed the exact
  Pico reader. JCAlgTest produced all 4,525 rows; the support comparator
  returned `COMPARE:PASS rows_statuses_and_declared_support` with 15 declared
  supported identities and no extra or missing entries. Documentation and
  artifact diff checked; no code tests were added.
- Result: CHOUPI statuses were 15 `yes`, 4,510 `no`, 0 other; jCardSim had
  1,454 additional `yes` capabilities, which are expected differences. CSV
  SHA-256 `4877cd4ab8205ae41721d5717bf46328a9a910805e0fc5655f60e7af235495ea`.
  Official client and serial captures are archived under `artifacts/logs/` and
  the result CSV under `artifacts/results/physical-alg-support-ownerpinx-20261020/`.
- Decision: AC4's post-OwnerPINX physical comparison is complete. Next address
  AC5 with an actual power-off/on persistence readback; do not infer that from a
  firmware reboot.
- Commit: `daf7ff3`
- Artifacts/logs: `artifacts/logs/jcalgtest-alg-support-ownerpinx-20261020.log`
  (SHA-256 `db06f0c398b7e7f091b663232c07d9a27af7d65cd124ba5a2d60d2b4cbae18c5`);
  `artifacts/logs/monitor-alg-support-ownerpinx-20261020.log` (SHA-256
  `dd2c20bfb0e70ea166debc12a9423f1bb7e2170dff7de2585c7e0bba1bbde8c6`);
  comparator log `artifacts/logs/jcalgtest-support-compare-ownerpinx-20261020.log`.

## 2026-10-20 — Prepare AC5 power-cycle validation baseline

- Step/experiment: `physical-jcvm-power-cycle-baseline`
- Change: Tried an isolated test CAP, found the loader's configured package-AID
  restriction, and recorded it in QKB. Removed the temporary host-tool edits;
  confirmed the existing JCAlgTest CAP was untouched. Updated AC5 to require a
  true power-off/on followed by the applet probe and support sweep.
- Validation: GPPro rejected the alternate package at `INSTALL [for load]`
  with `6D00` before any LOAD blocks. The existing applet then passed
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup` on the named reader:
  registry confirmed, SELECT `9000`, GETVERSION `1.8.2_jc305`, and JCSystem
  info `9000`. This is the pre-cycle baseline only; AC5 remains open.
- Result: The physical card is ready for the requested power-off/on check with
  its original JCAlgTest CAP. No token state was modified by the rejected
  alternate-AID install attempt.
- Decision: After a real power removal and reinsert, rerun the bounded power-up
  probe and repeat `ALG_SUPPORT_EXTENDED` to complete AC5 reproducibility.
- Commit: `8fe8ff0`
- Artifacts/logs: `artifacts/logs/gppro-install-jcsystem-persistence-probe-20261020.log`
  (SHA-256 `ca2fed0d6e487d1e007ecc671a2b95893bc6d30aec1b87220cabe0a134e8e678`);
  `artifacts/logs/pico-powerup-persistence-20261020T131336Z.log` (SHA-256
  `02bbc6ef3f3dd35ed3febe5c27c9550a7fc96637c8692616e24f6542e45570f1`).

## 2026-10-20 — Verify physical applet persistence after power cycle

- Step/experiment: `physical-jcvm-power-cycle-after-reconnect`
- Change: Recorded the post-disconnect physical evidence, marked AC5 complete in
  `TODO.md`, refreshed `STATUS.md`, and updated `.agent/HANDOFF.md` with the
  remaining integration work.
- Validation: Restored authorized USB access and confirmed the Pico PC/SC reader
  reappeared after the user's reported five-second disconnect/reconnect.
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup '<reader>'` passed: package
  and applet remained present, SELECT/GETVERSION/JCSystem returned `9000`, and
  GETVERSION returned `1.8.2_jc305`. The official JCAlgTest 1.8.2
  `ALG_SUPPORT_EXTENDED` client completed with exit status 0; the comparator
  returned `COMPARE:PASS rows_statuses_and_declared_support`. Serial capture
  completed. No code changed, so builds were not rerun.
- Result: All 4,525 support rows matched the reference identities. CHOUPI
  reported 15 `yes`, 4,510 `no`, and zero other statuses; all 15 declared
  supported algorithms matched. CSV SHA-256
  `b2b0d4d279ace0b2f663b4a79779afcce65c5fc9e0030768b635f4aac759ce8b`.
- Decision: AC5 persistence and reproducibility are verified on the physical
  Pico. Continue with AC1/AC2 clean-install evidence and the three-run
  clean-flash integration sequence. Unsupported algorithms remain expected.
- Commit: `457922b` (recorded in the metadata follow-up commit)
- Artifacts/logs: probe `artifacts/logs/pico-powerup-persistence-20261020T132635Z.log`
  (SHA-256 `96eb1ae56c155d4d1245586bc34bec3b437fb8f59b33e9e7337f06770086c4c0`);
  client `artifacts/logs/jcalgtest-alg-support-after-powercycle-20261020.log`
  (SHA-256 `591ab8e88c4ed8b1ac6abac45dae10a06dbd9942485c9fdbff76a6ac81b0f148`);
  serial `artifacts/logs/monitor-alg-support-after-powercycle-20261020.log`
  (SHA-256 `ffddea842eb48744e14687ef60a3f82c77d4ba35c4a2d361ec4214e67b421f64`);
  comparison `artifacts/logs/jcalgtest-support-compare-after-powercycle-20261020.log`
  (SHA-256 `945b10b7ea4c29aa50805bc94a4f88caf8169f6bb3de5f518d033cbc2fe1b882`);
  CSV under `artifacts/results/physical-alg-support-after-powercycle-20261020/`.

## 2026-10-20 — Reconcile current persistence status

- Step/experiment: `ac5-status-reconciliation`
- Change: Updated the current status and preceding handoff checkpoint so they
  reflect the completed physical power-cycle validation recorded above.
- Validation: Reviewed the AC5 probe and post-cycle support comparison records;
  `git diff --check` passed for the Markdown changes.
- Result: Current status pages agree that AC5 passed and retain broader CAP
  restore and clean-install integration work as open items.
- Decision: Continue with AC1/AC2 clean-install evidence and the three-run
  clean-flash integration sequence.
- Commit: `7b4ec20` (recorded in the metadata follow-up commit)
- Artifacts/logs: none; evidence is in the preceding AC5 progress entry.

## 2026-10-20 — Clean GlobalPlatformPro install on physical Pico

- Step/experiment: `physical-gppro-clean-install-jcalgtest-182`
- Change: Documented the clean Linux GlobalPlatform flow, closed AC2, and
  recorded the remaining Windows check for AC1 in `TODO.md`, `STATUS.md`,
  `.agent/HANDOFF.md`, and `.agent/QKB.md`.
- Validation: Physical `pcsc_scan -r` listed the named Pico. GPPro 26.06.04
  `--list` connected over T=1 and reported ISD `A000000151000000 (OP_READY)`.
  Deleted applet instance AID `4A43416C675465737431` and package AID
  `4A43416C6754657374` (each `9000`), confirmed the registry empty, then ran
  GPPro `--install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap`; all LOAD blocks
  and final INSTALL returned `9000`. `tools/test-pico-powerup '<reader>'`
  passed: package `LOADED`, applet `SELECTABLE`, SELECT `9000`, GETVERSION
  `1.8.2_jc305`, and JCSystem info `9000`. No source changed; builds were not
  rerun.
- Result: Clean install verified on Linux. CAP SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  The initial attempt to delete the package while its applet instance existed
  returned `6985`; deleting the instance first is the validated sequence.
- Decision: AC2 is complete. AC1 still needs Windows reader/GPPro validation;
  next run the plan's three-pass clean-flash sequence when applicable.
- Commit: `22167b6` (recorded in the metadata follow-up commit)
- Artifacts/logs: `artifacts/logs/gppro-clean-install-list-before-20261020.log`
  (SHA-256 `87712d58f8af691fe31ef20b19f58627bf66b16e3459abc6bc1635baccdbc3be`),
  `artifacts/logs/gppro-clean-install-delete-20261020.log`
  (`493cfc1e37332028fcfa1142d183feb569205ce00daaea54a36bc72d9db3103e`),
  `artifacts/logs/gppro-clean-install-delete-applet-20261020.log`
  (`27446d25d37d09dcb7551a3b8be69f5b64ee5db209bbbd614b88a1cf16d94e87`),
  `artifacts/logs/gppro-clean-install-delete-package-20261020.log`
  (`22181bb44d8996c0fd28f2e598aafa93d970a306574852c238c660b533906d9b`),
  `artifacts/logs/gppro-clean-install-list-empty-20261020.log`
  (`8491912b370d634c65a8bb7027750f739ac52e6564d119dc73bed3789a6d0bf6`),
  `artifacts/logs/gppro-clean-install-load-install-20261020.log`
  (`65b321477a5ddfc5e052d72cedc4da14fba2f13255592589fcb528b735f17fc3`), and
  `artifacts/logs/pico-powerup-persistence-20261020T135329Z.log`
  (`0c2c63bdec53c2ff4338f90e0413f0d2ee9b7a9c712e0f9363f42c85d73fc75a`).

## 2026-10-20 — Start clean-flash integration pass one

- Step/experiment: `clean-flash-integration-run1-pre-powercycle`
- Change: Started the plan's three-pass physical integration sequence after the
  clean GP install. Updated the current checklist and handoff to show run one's
  validated stages and remaining power-cycle stages.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The gated
  `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi` passed its ARM flash and
  18-second runtime capture (`TEST:PASS choupi_runtime`,
  `TEST:SUMMARY pass=1 fail=0`, no `FAULT`). GPPro listed the ISD after flash;
  the applet instance and package were deleted, the empty registry confirmed,
  and the supplied CAP was loaded/installed with all LOAD and final INSTALL
  responses `9000`. `tools/test-pico-powerup` passed. The official physical
  `ALG_SUPPORT_EXTENDED` run exited 0 and the comparator passed all 4,525 rows
  and the declared support list. Serial capture completed. No code changed.
- Result: Before the required power cycle, CHOUPI reported 15 `yes`, 4,510
  `no`, zero other statuses; all 15 declared supported algorithms matched.
  CSV SHA-256 `8d0c4acd6469d24246e467f1402c5230854f8154e6ea900107817cfa89bbd6a6`.
  The cycle log included one `CHOUPI:WATCHDOG_RECOVERY cause=other` line, but
  subsequent runtime checks passed and the wrapper exited 0.
- Decision: Run one remains in progress, not counted complete, until a true
  power removal is followed by a passing power-up probe and repeated support
  comparison. The user was asked to unplug/reconnect for about five seconds.
- Commit: `2eb1782` (recorded in the metadata follow-up commit)
- Artifacts/logs: build logs `artifacts/logs/build-arm-three-pass-integration-20261020.log`
  and `artifacts/logs/build-riscv-three-pass-integration-20261020.log`;
  cycle `artifacts/logs/cycle-choupi-20261020T140230Z.log`;
  GPPro logs `artifacts/logs/gppro-three-pass-run1-*.log`;
  power-up probe `artifacts/logs/pico-powerup-persistence-20261020T140519Z.log`;
  client `artifacts/logs/three-pass-run1-jcalgtest-20261020.log`;
  serial `artifacts/logs/three-pass-run1-monitor-20261020.log`;
  comparison `artifacts/logs/three-pass-run1-compare-20261020.log`;
  CSV under `artifacts/results/three-pass-run1-20261020/`.
## 2026-10-20 — Test deploy-folder API coverage Core stage on Pico

- Step/experiment: `physical-deploy-api-coverage-core-20261020`
- Change: Read the deployment README and attempted its first staged API coverage
  run against the physical Pico using the supplied Core CAPs and GPPro, instead
  of the JCAlgTest CAP. Recorded the package-AID loader limitation in QKB,
  updated the implementation backlog and handoff, and preserved the transcripts.
- Validation: The previously built/flashed ARM image passed an 18-second
  serial capture (`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, no
  `FAULT`). PC/SC listed the Pico reader; GPPro `--info` succeeded over T=1.
  README's `--areas Core --api auto` runner tried CAP versions 305, 304, 303,
  222, and 221. Every `INSTALL [for load]` returned `6D00`; no LOAD blocks or
  API calls occurred. A subsequent GPPro `--list` confirmed the original
  JCAlgTest package/applet remained and no API CAP was installed. No RISC-V
  build or test was run.
- Result: Core API coverage is blocked before CAP loading by the runtime's
  configured package-AID restriction. The failure is not an API test result.
  The card registry remained unchanged.
- Decision: Generalize CAP package acceptance and its storage/registry/cleanup
  lifecycle, then restart with the README's Core stage. Do not proceed to later
  API areas until Core loads successfully.
- Commit: `970002e`
- Artifacts/logs: serial `artifacts/logs/api-coverage-arm-monitor-20261020.log`
  (SHA-256 `4f295cb3911ceed398a612a8a48c4880fdd9a37daf145d74c9b6a9ba27de86ac`);
  GPPro preflight `artifacts/logs/api-coverage-gppro-info-20261020.log` (SHA-256
  `b64cca1aa575b02a78442abd5ad034fdf3a377ebe1d25a38e8877517404809e2`);
  runner `artifacts/logs/api-coverage-pico2-core-auto-20261020.log` (SHA-256
  `e40d79117cad3fa18e3ceba1fdc4d2e34db24cc0790f415a3af9c1e715cad8fb`), CSV
  `artifacts/results/api-coverage-pico2-core-auto-20261020.csv` (SHA-256
  `0f80b709479bf17e6175a1b9f99b929dc381c7805810e29c2edf8516a9d5bcf1`), and
  post-run registry `artifacts/logs/api-coverage-gppro-registry-after-20261020.log`
  (sanitized; SHA-256 `5f70481d7885c4a88c847a908d5c561c94a62115574d5a8878e7d76e91f60a4e`).

## 2026-10-20 — Add multi-package CAP flash slots

- Step/experiment: `multi-cap-slot-storage`
- Change: Replaced the single reserved CAP slot with eight package-indexed 68 KiB slots at flash offset `0x100000`, each storing a 64 KiB CAP maximum and package AID/code CRC metadata. Added package lookup, package write/delete/data, and per-package installed-instance metadata APIs. Retained ID-63 compatibility wrappers for the card-manager migration.
- Validation: `./tools/build arm` passed (`BUILD:PASS arch=arm`, `BUILD:PASS target=choupi`). The resulting firmware image ends at `0x100bb400`, below the slot region. RISC-V build and hardware flash were intentionally skipped under the ARM-only development scope and repository hardware build gate.
- Result: The storage layer now supports up to eight independent CAP images with fixed package IDs 56–63. Loader and VM callers are not migrated yet; arbitrary package AIDs are not end-to-end supported by this step.
- Decision: Continue by allocating non-conflicting dynamic package IDs and routing CAP parsing, loading, VM lookup, and deletion through the package-indexed APIs.
- Commit: `9c7b9ae`
- Artifacts/logs: build output from `./tools/build arm`; no hardware log

## 2026-10-20 — Accept arbitrary package AIDs and route package lifecycle

- Step/experiment: `arbitrary-cap-package-loader`
- Change: Removed the configured JCAlgTest package-AID check. CAP LOAD now
  rejects duplicate package AIDs, allocates an unused ID from 56–63, and stores
  CAPs by package. Added up to 40 applet-instance records per package, dynamic
  VM package lookup, package-aware install/restore/delete, and GET STATUS
  enumeration of loaded packages, declared applet modules, and installed
  instances. Package DELETE clears each recorded applet class's persistent
  state.
- Validation: `./tools/build arm` passed after a package DELETE status fix
  (`BUILD:PASS arch=arm`, `BUILD:PASS target=choupi`; final image end
  `0x100c56fc`). `./tools/build-pc` passed before that final shared-source
  guard; the final host target rebuilt its CAP loader but did not provide a
  completed command status. Registry tests passed 47/47, DELETE tests 26/26, and install-status
  tests 10/10. The two-pass PC VPCD applet test did not pass: restricted socket
  creation was denied, and the host-access retry timed out before VPCD
  connected. No RISC-V build, physical flash, or physical/API CAP exercise was
  run.
- Result: Arbitrary package identifiers and package-indexed runtime paths now
  compile for ARM and PC. Multiple package installation and deploy Core API CAP
  interoperability still need end-to-end validation. Existing atomicity gap
  between CAP metadata and filesystem transaction remains.
- Decision: Keep deploy-folder API coverage open until its Core CAPs complete
  upload, test, and cleanup; next test multiple package AIDs and lifecycle
  operations in an emulator or on the card.
- Commit: `e20944e`
- Artifacts/logs: Build output only. VPCD attempt used
  `artifacts/logs/host-vpcd-jcalgtest-20261014.log` and timed out before socket
  connection.

## 2026-10-21 — Restore physical multi-package INSTALL and SELECT

- Step/experiment: `physical-deploy-multi-package-305`
- Change: INSTALL now resolves the requested package from its APDU AID on every
  command, surviving GPPro power-on restoration of another package. Dynamic
  SELECT resolves the installed instance record to its package/app AID and the
  per-applet install offset. The deploy runner now checks explicit INSTALL
  failures before the `CAP loaded` success marker.
- Validation: `./tools/build arm` passed. Flashed with
  `PICO2_HOST_USB_ACCESS=1 ./tools/flash build/arm/pico2_choupi.uf2`; bounded
  `PICO2_HOST_USB_ACCESS=1 ./tools/monitor --seconds 18` passed
  `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. GPPro physical
  Multi package LOAD passed; separate applet A and B CREATEs and SELECT APDUs
  each returned `9000`; `gp --list` showed both selectable. Deploy Core API
  client ran with reader index 3 and completed 291 calls. Security completed
  901 calls; Crypto completed 466 direct calls after an INSTALL retry. The
  classifier check confirmed `CAP loaded` followed by `INSTALL failed: 6A82`
  returns `refused_install`.
- Result: Core/Multi packages and applets are installable and selectable across
  GPPro power-ons. Core API reported 236 OK, 15 expected unsupported, 6 wrong,
  18 runtime exceptions, 12 SW errors; overlapping array copies and OwnerPIN
  remain defects. Security reported 40 OK, 822 expected unsupported, 16
  runtime exceptions, 15 SW errors, 8 wrong. Crypto reported 13 OK, 444
  expected unsupported, 6 CryptoRuntimeException, 3 SW errors. DELETE returns
  `6581` for populated Core/Service applets but package-only cleanup succeeds.
- Decision: Keep deploy validation active; next fix Core Util overlap and
  OwnerPIN behavior, then isolate persistent applet DELETE `6581`. Later area
  CSVs record JavacardxExt (13 SW errors), JavacardxSec (84 SW errors),
  Javacardx/JavacardxInt INSTALL refusal `6A80`, Service (124 calls, 77 OK),
  and Biometry LOAD refusal `6D00`. The first Crypto runner CSV was a
  partial-install false positive; use the direct Crypto client CSV as evidence.
- Commit: `875bf99` (this ID is recorded in the metadata follow-up commit)
- Artifacts/logs: `artifacts/results/deploy/pico2-core-client-20261021.csv`,
  `pico2-core-with-ext-client-20261021.csv`,
  `pico2-security-20261021.csv`, `pico2-crypto-client-20261021.csv`,
  `pico2-javacardxext-20261021.csv`, `pico2-javacardxsec-20261021.csv`,
  `pico2-javacardx-20261021.csv`, `pico2-javacardxint-20261021.csv`, and
  `pico2-service-biometry-20261021.csv` under `artifacts/results/deploy/`.
  Physical serial log: `/tmp/pico2-deploy-core-monitor-selectfix.log`.


## 2026-10-21 — Fix physical Util overlapping copies

- Step/experiment: `physical-core-util-overlap`
- Change: Updated the Java Card SDK `Util.arrayCopyNonAtomic` implementation
  to copy backwards when source and destination are the same array and the
  destination overlaps at a higher offset. `arrayCopy` delegates to this
  implementation.
- Validation: `./tools/build arm` passed. Flashed the physical Pico 2 with
  `PICO2_HOST_USB_ACCESS=1 ./tools/flash build/arm/pico2_choupi.uf2`; the first
  reset-interface attempt failed, then the wrapper recovered through CDC
  BOOTSEL and flashing passed. `PICO2_HOST_USB_ACCESS=1 ./tools/monitor
  --seconds 18` passed repeated runtime checks (summary pass=1 fail=0).
  Deploy Core API 3.0.5 completed 291 calls on reader index 3; both
  overlapping-range copy calls returned the expected value and effect.
- Result: Same-array overlapping copies pass on hardware. This client run
  still reports OwnerPIN runtime/SW failures and a wrong Shareable interface
  result; some Ext scenario calls report APPLET_NOT_INSTALLED.
- Decision: Continue with the OwnerPIN constructor failure, then rerun the
  complete deploy stages and investigate persistent package DELETE `6581`.
- Commit: `9dae416` (evidence and progress); SDK submodule source commit
  `9d70825`.
- Artifacts/logs: `artifacts/results/deploy/pico2-overlap-retest-20261021.csv`,
  `artifacts/results/deploy/pico2-overlap-retest-client-20261021.log`; bounded
  monitor output was captured during this session but not archived separately.

## 2026-10-21 — Remap physical OwnerPIN and PIN dispatch

- Step/experiment: `physical-core-ownerpin-token-remap`
- Change: Added independent JC305-to-embedded token maps for OwnerPIN virtual
  methods and the PIN interface. The reference and embedded `framework.jca`
  method tables have different orders; using raw external method tokens caused
  constructor/method probes to fail or call the wrong method.
- Validation: `./tools/build arm` passed. Flashed with
  `PICO2_HOST_USB_ACCESS=1 ./tools/flash build/arm/pico2_choupi.uf2`; the
  wrapper recovered via CDC BOOTSEL after its reset-interface probe failed.
  `PICO2_HOST_USB_ACCESS=1 ./tools/monitor --seconds 8` passed repeated runtime
  checks. The physical deploy Core API 3.0.5 client completed 291 calls and all
  16 direct OwnerPIN calls and 12 PIN-interface calls passed.
- Result: OwnerPIN is no longer a physical Core failure. The complete direct
  Core run had 268 OK, 15 `UNSUPPORTED_PER_SPEC`, 3 UserException runtime
  probes, 4 Ext scenario `APPLET_NOT_INSTALLED` rows, and one Shareable wrong
  result. The Ext scenario requires its separate CAP; the single-context
  Shareable limitation is documented in `.agent/QKB.md`.
- Decision: Rerun Core through the deploy runner with Multi and Ext CAPs, then
  continue investigating persistent applet deletion and later API areas.
- Commit: `a577c25`.
- Artifacts/logs: `artifacts/results/deploy/pico2-ownerpin-map-retest-20261021.csv`
  and `pico2-ownerpin-map-client-20261021.log`.

## 2026-10-21 – Remap inherited framework lifecycle tokens

- Step/experiment: `physical-core-inherited-lifecycle-dispatch`
- Change: `Class_Handler::doGetPublicMethodOffset()` now remaps an imported
  virtual method token when lookup crosses from a dynamic CAP subclass to an
  external embedded framework superclass. This aligns inherited lifecycle
  calls such as `Applet.select()` with the framework's method table.
- Validation: `./tools/build arm` passed (image end `0x100c57b8` before the
  final diagnostic-free rebuild; final clean-source ARM build also passed,
  image end `0x100c57b8`). The physical Ext applet SELECT changed from `6999`
  to `9000`; deploy Core API 3.0.5 returned OK for inherited `Applet.select()`
  and `deselect()`. A bounded monitor reported repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. RISC-V was not
  built per the active ARM-only instruction. The latest clean image could not
  be reflashed because the Pico disappeared from the VM USB bus.
- Result: The inherited lifecycle lookup mismatch is fixed. The 291-call Core
  client still reports OwnerPIN protected-flag failures, three
  `UserException.throwIt` runtime failures, one Shareable wrong result, and
  missing Multi/Ext scenario applets; 11 `UNSUPPORTED_PER_SPEC` rows are
  expected. API cleanup returned `6581` for populated Core, Security, Crypto,
  and Service packages.
- Decision: Keep the superclass-boundary remap. Restore USB access, flash the
  clean build, reinstall Multi and Ext, then continue Core and later deploy
  areas. Diagnose populated-package deletion before relying on runner cleanup.
- Commit: `6c8b3ff`
- Artifacts/logs: Core CSVs
  `artifacts/results/deploy/pico2-selectfix_APISUPPORT_3b800181.csv` and
  `artifacts/results/deploy/pico2-dispatch-fix_APISUPPORT_3b800181.csv`;
  GPPro cleanup transcript `artifacts/logs/deploy-preclean-305-20261021.log`.

## 2026-10-21 — Remove the high-index CAP DELETE guard

- Step/experiment: `deploy-delete-high-class-index`
- Change: Removed the cleanup rejection for applet class indexes above 255 and
  made its conversion to the existing one-byte persistent-field tag explicit.
  Added a persistent-transaction regression using class index `0x1234`.
- Validation: `./tools/build arm` passed (`BUILD:PASS target=choupi`, image
  end `0x100c57b0`); `./tools/test-persistent-transaction` passed with
  `TEST:SUMMARY fail=0`; `./tools/test-gp-delete` passed 26/26. RISC-V was not
  built per the active ARM-only instruction. Physical access check found no
  reader (`sudo -n /usr/local/sbin/pico2-pcsc-reader`: found 0;
  `PICO2_HOST_USB_ACCESS=1 lsusb -d 2e8a:`: empty), so no flash, monitor, or
  physical applet DELETE was performed.
- Result: The implementation path no longer returns `6581` solely because a
  recorded class index exceeds 255. This matches the current byte-sized field
  tag ABI; possible collisions for same-package indexes separated by 256
  remain unresolved and physical DELETE confirmation is pending.
- Decision: Reflash the clean ARM build when the board returns, delete a
  populated deploy CAP, confirm GET STATUS and reinstall, then continue the
  remaining deploy README stages.
- Commit: `2c3459e` (implementation); progress record commit follows.
- Artifacts/logs: Build output `build/arm/pico2_choupi.uf2`; host test output
  was not archived separately.

## 2026-10-21 — Preserve full-width CAP class indexes in persistent tags

- Step/experiment: `persistent-tags-class-index-16bit`
- Change: Kept the legacy five-byte applet-field root for class indexes through
  255 and added a six-byte root containing the complete class index above 255.
  Added legacy-key fallback for reads and deletion of both encodings. Filesystem
  overlay and `JC_Instance` tag compression now recognize both roots.
- Validation: `./tools/build-pc` passed and refreshed ROM mask output;
  `./tools/build arm` passed (image end `0x100c5c3c`); persistent transaction,
  RP2350 filesystem overlay, GP registry (47/47), install status, and GP DELETE
  (26/26) regressions passed. Deploy `--list --api 305` found Core, Security,
  Crypto, Javacardx, JavacardxExt, JavacardxSec, JavacardxInt, Service, and
  Biometry CAPs. VPCD JCAlgTest SELECT passed, but GETVERSION timed out at 20 s
  and again at 60 s; the test's CAP-load and empty-flash fixtures were still
  dated October 14, so that run is inconclusive. The authorized PC/SC helper
  reported `found 0`; no physical upload or deploy API calls occurred. RISC-V
  was not built per the active ARM-only scope.
- Result: Class indexes 1 and 257 map to distinct persistent keys; legacy
  values remain readable and high-index cleanup removes both tag forms. The
  focused package registry and delete regressions still pass. Multi-applet
  installation and deploy API coverage remain pending physical access.
- Decision: Regenerate the VPCD fixture set before relying on the JCAlgTest
  timeout, restore Pico USB visibility, then upload the deploy Multi and Ext
  CAPs and test both Multi applets before proceeding through the README areas.
- Commit: `c52e9b9` (implementation); nested OS path encoder commit `7e9d068`.
  `f964152` additionally captured pre-existing `JC_Instance` diagnostic edits
  while repairing a file-mode mistake; those edits were preserved in that
  commit.
- Artifacts/logs: `artifacts/logs/high-index-tag-jcalgtest-vpcd-20261021.log`,
  `artifacts/logs/high-index-tag-jcalgtest-vpcd-20261021-retry.log`, and
  `build/arm/pico2_choupi.uf2`.

## 2026-10-21 — Recheck deploy access and current VPCD applet path

- Step/experiment: `physical-multi-applet-access-recheck`
- Change: No implementation change. Rechecked physical visibility, verified
  deploy API 3.0.5 assets, and reran package/install/delete host regressions.
- Validation: PC/SC helper returned `Expected exactly one Pico 2 CHOUPI CCID
  (2e8a:0009); found 0`. Deploy `--list --api 305` found all nine areas.
  GP registry passed 47/47, install status 10/10, and GP DELETE 26/26. A fresh
  512 KiB erased image plus CAP code assembled directly from
  `AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap` reached ATR and JCAlgTest SELECT
  (`9000`), then GETVERSION timed out at 30 s. VPCD startup was 12.368 s. No
  deployment Multi/Ext CAP was uploaded because neither a real reader nor a
  VPCD GlobalPlatform bridge is available. RISC-V was not built.
- Result: The new persistent-tag and registry regressions pass. The current PC
  VPCD method-execution path has an unresolved GETVERSION timeout even with
  exact CAP code and freshly erased storage. This does not establish a physical
  regression. Physical Multi/Ext and API-area execution remain unverified.
- Decision: Keep the physical hardware request open. When the device appears
  in the VM, build/flash ARM under the repository gate, run the deploy Core
  stage (which loads Multi and creates both `...41` and `...42` applets), then
  proceed through Security and Crypto.
- Commit: `bfe6755` records the prior validation checkpoint.
- Artifacts/logs: `artifacts/logs/jcalgtest-exact-cap-vpcd-20261021.log`;
  generated fixtures `build/algtest-v182-current-{empty-flash,load-code}.bin`.

## 2026-10-21 — Correct checkpoint commit reference

- Step/experiment: `progress-commit-reference-correction`
- Change: Clarified the commit reference in the preceding progress entry; it is
  contained in commit `26adbb5`, not `bfe6755`.
- Validation: Inspected `git show --stat 26adbb5`; the commit contains the
  preceding progress, handoff, and QKB updates.
- Result: The prior entry now has an auditable commit reference without
  rewriting the append-only progress log.
- Decision: Continue with the same pending physical Multi/Ext test when Pico
  USB appears in the VM.
- Commit: `26adbb5` (commit that this correction identifies).
- Artifacts/logs: none.

## 2026-10-21 — Recheck physical access and multi-package test coverage

- Step/experiment: `physical-multi-applet-retest-access-check`
- Change: No runtime code change. Rechecked BOOTSEL device visibility, reran
  package registry/install/delete regressions, confirmed the deploy API 3.0.5
  CAP inventory, and inspected the host VPCD storage boundary.
- Validation: The authorized PC/SC helper reported `found 0`; gated
  `lsusb -d 2e8a:` returned empty. `./tools/test-gp-registry` passed 47/47,
  `./tools/test-install-status` passed all 10 checks, and
  `./tools/test-gp-delete` passed 26/26. Deploy `./run_api_test.sh --list
  --api 305` found GPPro, client, and all nine areas; it printed three
  non-fatal stream-fd permission messages. No physical LOAD/INSTALL/APDU or
  emulator multi-package test was run.
- Result: Current host VPCD `main_pc.cpp` stores one package and one applet
  instance, so it cannot prove multi-package behavior. Hardware flash and
  multiple-CAP validation remain blocked by USB enumeration.
- Decision: Resume on the physical card once it enumerates: run Core scenarios,
  verify both Multi instances, install a second package concurrently, test
  SELECT/GET STATUS/DELETE/reinstall, and proceed with remaining deploy stages.
- Commit: to be recorded in a follow-up reference correction.
- Artifacts/logs: none; existing physical Core CSVs remain under
  `artifacts/results/deploy/`.

## 2026-10-21 — Record physical retest checkpoint commit

- Step/experiment: `physical-multi-applet-retest-access-check-commit-reference`
- Change: Added the committed checkpoint identifier to the preceding progress
  record without changing its validation results.
- Validation: Inspected commit `dfacb94`; it contains the handoff, QKB, and
  progress checkpoint updates.
- Result: The physical-access checkpoint is now traceable to its commit.
- Decision: Resume physical Multi and deploy CAP testing when USB enumeration
  returns.
- Commit: `dfacb94` (checkpoint); this correction is committed separately.
- Artifacts/logs: none.

## 2026-10-21 — Fix imported class checks and smoke-test deploy CAPs

- Step/experiment: `deploy-core-userexception-vpcd`
- Change: `Bytecodes::docheck()` now preserves a valid imported class reference
  when its alternate JC305 interface-token interpretation throws
  `SecurityException`. Added a focused probe for `UserException.throwIt()` caught
  as `Exception`, followed by `instanceof UserException` and reason validation.
  Updated TODO, QKB, and handoff with the root cause and physical retest state.
- Validation: `./tools/build-pc` passed in default mode;
  `CHOUPI_JCVM_DEBUG=ON ./tools/build-pc` passed for diagnosis;
  `./tools/build arm` passed (`BUILD:PASS target=choupi`, image end
  `0x100c5c7c`). VPCD focused case 78 passed with response `01 9000` and
  `TEST:SUMMARY pass=3 fail=0`. The deploy Core CAP returned ATR `3B800181`,
  SELECT `9000`, and UserException probe data `00000001` / SW `9000`. Isolated
  deploy 3.0.5 CAP load/install/select passed for Core, Security, Crypto,
  JavacardxExt, JavacardxSec, Service, and Biometry; Javacardx and JavacardxInt
  were refused with install `6A80`, consistent with the README limitations.
  GP registry passed 47/47, install-status passed 10/10, GP DELETE passed
  26/26, and deploy asset listing found all nine areas. Full transaction VPCD
  default release emulator also passed the deploy Core `UserException.throwIt`
  APDU (`000000019000`). The full transaction VPCD suite passed cases 1–12 but
  was interrupted while waiting on case 13; the
  isolated case 13 passed in 59.686 seconds. Physical access check reported no
  Pico (`pico2-pcsc-reader: found 0`, gated `lsusb -d 2e8a:` empty); no hardware
  flash or APDU validation was possible. No RISC-V build was run.
- Result: The exception dispatch failure is fixed and reproduced against the
  supplied deploy Core CAP on the host emulator. Deploy packages that depend on
  API packages absent from the ROM remain refused as expected. Multiple packages
  were not tested concurrently because the PC VPCD shim models one package and
  instance; concurrent package retention and Multi applet behavior still need
  physical validation.
- Decision: On USB enumeration, flash this ARM build and run the official Core
  scenarios, confirm both Multi AIDs, install a second distinct package while
  the first remains, then continue Security/Crypto and remaining README stages.
- Commit: `bcef65f`.
- Artifacts/logs: `artifacts/logs/userexception-supercatch-vpcd-20261021-fix1.log`,
  `artifacts/logs/deploy-core-userexception-vpcd-20261021.log`,
  `artifacts/logs/deploy-core-userexception-vpcd-release-20261021.log`,
  `artifacts/logs/deploy-{core,security,crypto,javacardxext,javacardx,javacardxint,service,biometry,javacardxsec}-select-vpcd-20261021.log`,
  `artifacts/logs/jcsystem-transaction-full-vpcd-20261021.log`, and
  `artifacts/logs/jcsystem-ownerpinx-isolated-vpcd-20261021.log`.

## 2026-10-21 — Correct imported class-check commit reference

- Step/experiment: `deploy-core-userexception-vpcd-commit-reference`
- Change: Resolved the pending commit marker in the preceding progress record;
  the implementation, focused regression, TODO, QKB, and handoff were committed
  together.
- Validation: Inspected commit `58e92e6` and confirmed it contains the seven
  files for the imported class-check fix and deploy CAP smoke tests.
- Result: The implementation step now has an auditable Git reference.
- Decision: Continue with physical Core/Multi testing when USB enumeration
  returns; the hardware remains unavailable in this VM.
- Commit: `58e92e6` (implementation commit referenced by this correction).
- Artifacts/logs: none.

## 2026-10-21 — Exercise both deploy Multi CAP applets in VPCD

- Step/experiment: `deploy-multi-applet-independent-vpcd`
- Change: No runtime code change. Installed and selected each of the two applet
  AIDs declared by the deploy `AlgTestApiMulti_305.cap` in its own fresh host
  emulator image. Updated TODO and handoff with the exact result and remaining
  coexistence limitation.
- Validation: AID `4A43416C675465737441706D41` installed and selected with `9000`;
  AID `4A43416C675465737441706D42` installed and selected with `9000`. The
  authorized BOOTSEL visibility check still reports no physical Pico, so these
  are host VPCD results only. No simultaneous instances or multiple loaded
  packages were exercised.
- Result: Both package applet entry points work independently in the emulator.
  This does not establish persistence of both instances together or the GP
  LOAD/CREATE/DELETE lifecycle.
- Decision: On physical access, run the deploy Core scenarios, verify both AIDs
  coexist and are independently selectable, then load another distinct
  package without deleting Multi and check GET STATUS, DELETE, and reinstall.
- Commit: `3d583bc`.
- Artifacts/logs: `artifacts/logs/deploy-multi-a-select-vpcd-20261021.log` and
  `artifacts/logs/deploy-multi-b-select-vpcd-20261021.log`.

## 2026-10-21 — Correct Multi VPCD validation commit reference

- Step/experiment: `deploy-multi-applet-independent-vpcd-commit-reference`
- Change: Resolved the pending commit marker for the preceding Multi applet
  smoke-test record.
- Validation: Inspected commit `8dcd7a1`; it contains the TODO/handoff updates
  and the append-only test record.
- Result: The independent Multi A/B emulator check is traceable to its commit.
- Decision: Physical coexistence and second-package validation remain open.
- Commit: `8dcd7a1` (Multi smoke-test commit referenced by this correction).
- Artifacts/logs: none.

## 2026-10-21 — Handoff unresolved deploy Shareable dispatch

- Step/experiment: `deploy-core-shareable-dispatch-handoff`
- Change: Removed temporary `INVOKEVIRTUAL` diagnostic output and documented
  the current Shareable callback/interface-dispatch failure, partial diagnosis,
  validation state, hardware availability, and next bounded diagnostic in
  `.agent/HANDOFF.md` and `.agent/QKB.md`.
- Validation: Inspected the focused source diff and confirmed the temporary
  `DEBUG:INVOKEVIRTUAL` instrumentation is absent. No build or test was run
  during handoff; user requested stopping and the focused Shareable APDU is
  already known to fail. Host VPCD SELECT returned `9000`, but Shareable APDU
  `B083210000` returned `04 9000`. The ARM build passed before the final
  wrapper/resolver changes only. Authorized USB visibility check found no Pico;
  no hardware operation was attempted.
- Result: Shareable support remains incomplete. The interface-resolver fix is
  plausible but not confirmed end-to-end. The working tree has extensive
  unrelated changes and this failing implementation is left uncommitted.
- Decision: Resume with the narrow interface-table/callback trace, rerun the
  VPCD APDU, then build ARM and RISC-V as required before any hardware flash.
- Commit: `handoff documentation commit; SHA recorded by the following append-only correction`.
- Artifacts/logs: `/tmp/deploy-shareable-vpcd-diag.log`,
  `/tmp/deploy-shareable-vpcd-final.log` (availability not guaranteed).

## 2026-10-21 — Record Shareable handoff commit

- Step/experiment: `deploy-core-shareable-dispatch-handoff-commit-reference`
- Change: Recorded the handoff documentation commit identifier without
  changing the implementation or its validation state.
- Validation: Confirmed commit `93bdacb8592693c450e4c1d6e4879e3cb22968e0`
  contains only `.agent/HANDOFF.md`, `.agent/QKB.md`, and `progress.md`.
- Result: The incomplete Shareable implementation remains uncommitted while
  the handoff notes are traceable.
- Decision: Resume from the unresolved Shareable APDU diagnosis recorded above.
- Commit: `pending`.
- Artifacts/logs: none.

## 2026-10-21 — Reassess functionality review against current source

- Step/experiment: `functionality-review-update-20261021`
- Change: Added `review_20261021.md`, updated handoff and QKB, and identified
  overlap between the new CAP3 slot region and both RP2350 filesystem snapshots.
  Reassessed the four findings from `review_20261019.md` against current code
  and intervening progress records. No source files changed.
- Validation: `./tools/test-vpcd-session` passed 18/18;
  `./tools/test-short-apdu`, `./tools/test-persistent-transaction`, and
  `./tools/test-rp2350-fs-overlay` exited 0, the latter reporting
  `TEST:SUMMARY fail=0`; `./tools/test-gp-delete` passed 26/26 and
  `./tools/test-gp-registry` passed 47/47. Source address arithmetic confirmed
  CAP `0x100000..0x188000` overlaps FS `0x100000..0x180000` by `0x80000`.
  Builds, flash, USB/PCSC, and physical APDUs were intentionally skipped for
  this documentation-only review.
- Result: AID restore and SELECT continuation fixes exist, with remaining
  targeted coverage gaps. Cross-store recovery and SCP03 key provisioning
  remain open. The flash-map overlap is a new source-confirmed blocker; no
  physical corruption was measured or claimed.
- Decision: Correct and migrate the flash layout before further physical
  CAP/FS writes; then test combined CAP/filesystem recovery and finish focused
  AID/SELECT regressions. Preserve all pre-existing uncommitted source work.
- Commit: this documentation commit (`git log -1 --format=%H -- review_20261021.md`).
- Artifacts/logs: `review_20261021.md`; no new runtime logs archived.

## 2026-10-21 — Separate CAP3 and JFS2 flash regions and migrate snapshots

- Step/experiment: `flash-layout-jfs2-migration`
- Change: Added a shared 2 MiB flash map; retained eight CAP3 slots at
  `0x100000..0x188000`; moved JFS2 to two `0x3c000` slots at
  `0x188000..0x200000`. Startup migrates a valid legacy JFS2 snapshot without
  erasing its source and fails closed on unrecoverable snapshots or CAP2 magic.
  Expanded the fake-flash suite to link the actual CAP3 store and documented
  recovery limits. The focused source commit also includes pre-existing
  filesystem parser repairs needed for the currently validated source state;
  unrelated dirty source hunks remain untouched.
- Validation: `./tools/test-rp2350-fs-overlay` passed with `TEST:SUMMARY
  fail=0`, including all eight CAP slots, restart, migration, four power-cut
  points, and fail-closed cases. `./tools/build arm` and `./tools/build riscv`
  passed; ARM was rebuilt after the final source edit. Authorized read-only
  `PICO2_HOST_USB_ACCESS=1 lsusb -d 2e8a:` found no device (exit 1), so flash
  and serial validation were not run. The host harness emitted a pre-existing
  `mallinfo()` deprecation warning; firmware builds emitted no warnings.
- Result: The configured flash regions no longer overlap, and a valid old
  JFS2 snapshot has a retryable migration path. Data already erased by the
  old overlap cannot be reconstructed; CAP2 conversion is not implemented.
- Decision: Preserve a full card flash backup before a physical firmware
  upgrade, then verify migration and stored applets on hardware. Continue
  separate cross-store CAP/filesystem transaction recovery work afterward.
- Commit: this step's commit (`git log -1 --format=%H -- src/flash_layout.hpp`).
- Artifacts/logs: `docs/flash-layout-migration.md`; no hardware log.

## 2026-10-21 — Back up Pico and attempt physical layout test

- Step/experiment: `flash-layout-first-physical-cycle`
- Change: Made a verified full-flash backup before testing the layout
  migration. No source files changed. Inspected snapshot and CAP headers in
  the backup to establish the pre-flash recovery state.
- Validation: Authorized `lsusb -d 2e8a:` saw RP2350 BOOTSEL. `picotool save
  -a -v` completed and verified 2,097,152 bytes. Old JFS2 generations 11747
  and 11746 had valid header/payload CRCs; four CAP3 headers and a valid CAP2
  package 20 image were present. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi`
  passed ARM and RISC-V builds, but flash exited 1 after `picotool` reached
  100% and aborted with `picoboot::connection_error`. Recovery found no USB
  device; no serial monitor ran.
- Result: Hardware migration is unverified and the device's post-transfer
  state is unknown. CAP2 is valid and distinct, so the new firmware's
  fail-closed CAP2 guard is expected to stop startup until preservation or
  conversion is resolved.
- Decision: Wait for a BOOTSEL reconnect, compare device flash with the
  backup, then diagnose the runtime without erasing CAP2. Preserve the backup
  outside commits and do not treat 100% transfer as flash success.
- Commit: this experiment's documentation commit (`git log -1 --format=%H -- progress.md`).
- Artifacts/logs: `artifacts/backups/flash-before-layout-migration-20261021.bin`
  (SHA-256 `408ba9dff6edf6924830d6ae9e9860a1aa7fef40fe6976ebe9a2ee736e9bc4b6`),
  `artifacts/logs/cycle-choupi-20261021T091042Z.log`.

## 2026-10-21 — Preserve legacy CAP2 bytes in revised JFS2 layout

- Step/experiment: `flash-layout-cap2-preservation`
- Change: Narrowed each JFS2 slot to `0x28000` and placed them at
  `0x188000..0x1b0000` and `0x1c8000..0x1f0000`, outside CAP3 and CAP2's
  maximum 32 KiB header/code extent. Removed the CAP2 startup guard and
  revised the migration documentation and fake-flash test.
- Validation: `./tools/test-rp2350-fs-overlay` passed `TEST:SUMMARY fail=0`,
  including migration and a subsequent second-slot sync with simulated CAP2
  bytes unchanged. `./tools/build arm` and `./tools/build riscv` passed with
  no firmware warnings; the host harness emitted the pre-existing `mallinfo()`
  deprecation warning. Physical test of this revision awaits BOOTSEL.
- Result: The revised layout preserves valid legacy CAP2 storage bytes while
  allowing both JFS2 snapshot slots to operate. The CAP2 package remains
  inactive under CAP3; this step does not convert package IDs or VM state.
- Decision: Reflash the revised ARM image after device reattachment, then
  verify migration on serial and by reading back flash. Keep both full backups.
- Commit: this step's commit (`git log -1 --format=%H -- src/flash_layout.hpp`).
- Artifacts/logs: `docs/flash-layout-migration.md`; pre-flash backup SHA-256
  `408ba9dff6edf6924830d6ae9e9860a1aa7fef40fe6976ebe9a2ee736e9bc4b6`;
  post-transfer backup SHA-256
  `63076672521354b59baea9392031344b18470a3665c14b4d649324ee58f09b37`.

## 2026-10-21 — Verify CAP2-preserving JFS2 migration on Pico 2

- Step/experiment: `flash-layout-cap2-physical-migration`
- Change: Flashed the validated ARM layout revision to the BOOTSEL Pico,
  migrated its existing JFS2 state on boot, and captured a verified full-flash
  image for exact before/after comparison. No source files changed.
- Validation: `picotool load -v` verified the ARM UF2 before reboot. The first
  30-second monitor captured 30 `TEST:PASS choupi_runtime` and 30 matching
  summaries; the post-migration eight-second reboot captured eight of each.
  Both logs had zero `FAULT` and zero `TEST:FAIL`. `picotool save -a -v`
  verified the 2 MiB post-migration image. Its JFS2 slot 0 at `0x188000`
  passed header and payload CRCs at generation 11748; its 51,492-byte
  payload was identical to the old generation-11747 snapshot (2,264 records,
  28,852 overlay bytes). The entire CAP3/old-JFS2 range and preserved CAP2
  extent were byte-for-byte unchanged. GPPro `--list` exited 0 after reboot
  with four selectable applets and four loaded packages.
- Result: Physical migration and restart passed. The one-time
  `CHOUPI:FS_MIGRATE` line was missed during CDC attachment, but verified
  flash readback proves the migration. JFS2 slot 1 remains blank; its physical
  write path was not exercised. `test-pico-powerup` exited 1 because it expects
  a different hard-coded JCAlgTest AID than the four installed deploy applets;
  its GPPro registry query succeeded.
- Decision: Keep the revised firmware and the full backups. CAP2 bytes remain
  preserved but its package ID 20 is not converted to active CAP3 metadata.
  Continue separate cross-store transaction recovery work later.
- Commit: this evidence commit (`git log -1 --format=%H -- progress.md`).
- Artifacts/logs: `artifacts/logs/flash-layout-cap2-preserved-load-20261021.log`,
  `artifacts/logs/flash-layout-cap2-preserved-runtime-20261021.log`,
  `artifacts/logs/flash-layout-post-migration-reboot-20261021.log`,
  `artifacts/logs/flash-layout-post-migration-registry-20261021.log`,
  `artifacts/backups/flash-after-jfs2-migration-20261021.bin` (SHA-256
  `51a303f6f00da7611737b4e2a7b174ec59e7d793b6efbcb25002daab0adf6391`),
  and the pre-flash backup (SHA-256
  `408ba9dff6edf6924830d6ae9e9860a1aa7fef40fe6976ebe9a2ee736e9bc4b6`).

## 2026-10-21 — Correct dynamic applet virtual-token dispatch

- Step/experiment: `deploy-core-inherited-token-on-pico`
- Change: Deferred imported virtual-token remapping for dynamic receivers to
  the embedded framework superclass boundary, avoiding a second translation.
- Validation: `./tools/build arm` and `./tools/build riscv` passed after the
  final edit. A full `./tools/build-pc` attempt failed on an intermediate edit,
  corrected before firmware builds. Physical Core 3.0.5 runner loaded Core,
  Multi (two instances), and Ext and cleaned all three. `./tools/cycle choupi`
  built both targets but picotool exited 1 after 100% transfer; a read-only
  BOOTSEL `picotool verify` of the exact UF2 returned `OK`. The verified image
  rebooted and an eight-second serial monitor reported seven `TEST:PASS`
  runtime lines, no `TEST:FAIL` or `FAULT`.
- Result: Core's 291-call result improved from 256 OK, 14 runtime exceptions,
  two status errors, and 19 allowed unsupported to 267 OK, two runtime
  exceptions, three status errors, and 19 allowed unsupported. Nine inherited
  Applet method failures cleared. The five remaining failures are OwnerPIN
  validated flag (three) and Shareable (two). No throughput metric measured.
- Decision: Keep the verified token fix. Probe the remaining deploy CAP areas;
  diagnose OwnerPIN and Shareable separately.
- Commit: this step's commit (`git log -1 --format=%H -- choupi/src/jc_handlers/jc_class.cpp`).
- Artifacts/logs: `artifacts/results/deploy/pico2-deploy-core-20261021.csv`,
  `artifacts/results/deploy/pico2-deploy-core-tokenfix-20261021.csv`,
  `artifacts/logs/deploy-inherited-token-fix-verified-boot-20261021.log`;
  verified UF2 SHA-256
  `552c2b5a79c3669a24ed70b627ab8c935a3fb404d004c90b20b10cbe32f039a6`.

## 2026-10-21 — Handoff Shareable and deploy multi-package investigation

- Step/experiment: `deploy-shareable-handoff`
- Change: Appended the current investigation state, test results, limitations, and concrete continuation steps to `.agent/HANDOFF.md`; no implementation code changed in this handoff step.
- Validation: Reviewed `.agent/SESSION_CONTINUITY.md`, current handoff, relevant diffs, and existing progress records. ARM build had passed before PC-only trace instrumentation; `./tools/build-pc`, Python syntax check, and shell syntax check passed. No new tests or hardware commands were run for this documentation-only step.
- Result: The physical Core runner previously completed 291 calls (267 OK, 19 allowed unsupported, 3 status errors, 2 runtime exceptions); VPCD reproduces Shareable probe response `049000`. Latest working tree includes temporary PC-only traces and extensive unrelated dirty files.
- Decision: Stop here as requested. Next session should resolve package 17 PC 367 / thrown package 15 class 72, remove temporary traces, and respect the ARM-only constraint and flash build gate.
- Commit: `0e8f47f` (handoff and progress record only).
- Artifacts/logs: listed in `.agent/HANDOFF.md`; none created by this documentation step.

## 2026-10-21 — Handoff completed for Shareable dispatch fix

- Step/experiment: `shareable-dispatch-handoff`
- Change: Updated `.agent/HANDOFF.md` with the resolved dynamic virtual-token mismatch, implementation outline, exact host validation, hardware/build restrictions, and next steps. This supersedes the earlier stale diagnostic instructions in that file.
- Validation: Documentation/state review only; no builds, emulator tests, or hardware operations run for this handoff.
- Result: The handoff now records successful host and ARM builds plus focused VPCD Shareable and applet-selection checks. Updated source has not been flashed or physically validated. Extensive unrelated worktree changes remain preserved.
- Decision: Stop work as requested. Resume with physical validation only when both the ARM-only user constraint and repository flash gate can be reconciled; otherwise continue offline.
- Commit: unavailable; `git commit` could not create `.git/index.lock` because `.git` is mounted read-only in this session.
- Artifacts/logs: see the validation log paths and result CSV listed in `.agent/HANDOFF.md`.


## 2026-10-21 — Handoff for physical deploy API coverage

- Step/experiment: `deploy-api-physical-handoff`
- Change: Appended current build, physical API stage results, capacity finding, constraints, and next actions to `.agent/HANDOFF.md`. No implementation source changed in this handoff step.
- Validation: Reviewed `.agent/SESSION_CONTINUITY.md`, the existing chronological handoff, current progress state, worktree status, and all three supplied redacted session digests. The digests contain only operational metadata and were not treated as canonical knowledge. No build, test, or hardware operation was run for this documentation-only step.
- Result: Latest recorded `./tools/build-pc` and `./tools/build arm` passed; no RISC-V build or current-source flash occurred. Physical deploy areas have archived CSV/log evidence and unresolved API failures as itemized in the handoff.
- Decision: Stop work as requested. Next session should classify deploy expected outcomes, investigate focused framework/KeyBuilder failures, and honor both the ARM-only constraint and the repository flash gate.
- Commit: Not attempted; prior session reports `.git` read-only, and this checkout already has extensive unrelated dirty changes.
- Artifacts/logs: listed in `.agent/HANDOFF.md`; none created in this handoff step.

## 2026-10-21 — Handoff for Security KeyBuilder investigation

- Step/experiment: `security-keybuilder-host-handoff`
- Change: Appended current VPCD failure evidence, temporary trace locations, validation limits, and concrete next steps to `.agent/HANDOFF.md`. No implementation was changed.
- Validation: Reviewed prior session evidence and current trace-marker locations. No build, test, or hardware operation was run for this documentation handoff.
- Result: Current host reproduction reports `StackUnderflowException` for the Security DES KeyBuilder probe; the CAP method/offset mismatch is unresolved. Temporary diagnostic prints remain in `bc_invoke.cpp` and must be removed after diagnosis.
- Decision: Stop as requested. Resume with CAP Method offset and handler-table accounting; keep ARM-only and no-flash-under-build-gate constraints.
- Commit: Not attempted; prior session reports `.git` read-only in this environment. Only the two handoff files were edited.
- Artifacts/logs: `artifacts/logs/deploy-security-keybuilder-des-probe-vpcd.log` and related `*-debug/trace*-vpcd.log` files are listed in `.agent/HANDOFF.md`.

## 2026-10-21 — Fix JC305 Security Key interface dispatch

- Step/experiment: `security-key-interface-token-map`
- Change: Added the JC305 Security `Key` interface token map `{1, 3, 2, 0}` in `remap_imported_interface_method_token`. Removed the temporary `TRACE_STATIC_*` instrumentation from `bc_invoke.cpp` while preserving its existing dispatch changes. Updated TODO, QKB, and handoff with the confirmed mismatch and limits.
- Validation: `./tools/build arm` passed (RP2350 ARM image end `0x100c6430`); `./tools/build-pc` passed with `ROMMASK:PASS`. VPCD installed the actual `AlgTestApiSecurity_305.cap`; DES KeyBuilder APDU `B0 83 60 01 01 00 00` returned `000040019000`. Summary was `TEST:SUMMARY pass=11 fail=0`. No hardware command or RISC-V build was run.
- Result: A JC305 `Key.getSize` call previously resolved to embedded `KeyImpl.clearKey`, causing `StackUnderflowException`; the focused row now reports 64-bit size and not-initialized status. The offset resolver was checked against capmap-free parsing and is not implicated. Six exploratory row-index APDUs returned `9000`, but payloads such as `FC` are unclassified.
- Decision: Keep the scoped Key interface mapping. Continue classifying Security rows and run deploy Core/Security API tests on VPCD. Physical validation remains gated by the ARM-only versus both-build instruction conflict.
- Commit: Not attempted; `.git` metadata is read-only in this environment, and the source/docs contain unrelated pre-existing worktree changes.
- Artifacts/logs: `artifacts/logs/deploy-security-keybuilder-des-remap-vpcd-20261021.log`; `artifacts/logs/deploy-security-keybuilder-types-vpcd-20261021.log`; extracted Security class inspection used `/tmp/security-class`.

## 2026-10-21 — Current handoff after Security KeyBuilder fix

- Step/experiment: `security-keybuilder-session-handoff`
- Change: Appended the latest source state, incomplete Core VPCD probe status, active ARM-only/build-gate constraints, and next continuation steps to `.agent/HANDOFF.md`. No implementation source changed.
- Validation: Documentation review only. Confirmed the Core VPCD log ends after applet installation and connection without an APDU response or test summary; confirmed no matching VPCD test process remained. No build, test, or hardware operation was run for this handoff.
- Result: The Security `Key` token translation remains the latest confirmed fix. The new Core Shareable probe is inconclusive and requires inspection/rerun on resume. Extensive prior worktree changes remain intact.
- Decision: Stop work as requested. Resume by checking the incomplete Core log, then continue focused deploy API validation. Do not flash while the ARM-only request conflicts with the repository's both-architecture flash gate.
- Commit: Not attempted; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/deploy-core-shareable-current-source-vpcd-20261021.log`; Security validation logs listed in `.agent/HANDOFF.md`.

## 2026-10-21 — Deploy Core Shareable and Security KeyBuilder VPCD regressions

- Step/experiment: `deploy-core-security-vpcd-followup`
- Change: Revalidated both Core Shareable paths against the deploy Core CAP and corrected six persistent DES/AES KeyBuilder row probes against the deploy Security CAP. Updated TODO, QKB, and handoff with actual response bytes and physical-validation limits; no source code changed.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-pc-vpcd-applet --passes 1 --apdu-timeout 90 ...` ran through the local VPCD harness. Core own-AID response `00009000`; interface response `0001019000`; each focused session had `TEST:SUMMARY pass=11 fail=0`. Security row payloads were `00004001`, `00008001`, `0000C001`, `00008001`, `0000C001`, `00010001`, all followed by `9000`; summary `pass=11 fail=0`. No ARM build, RISC-V build, USB/PCSC operation, or physical test was run.
- Result: The Core Shareable fix passes the two focused host probes. The Security interface mapping now verifies six persistent DES/AES sizes instead of only the original DES-64 row. These checks validate response behavior on host VPCD, not physical firmware or concurrent multi-package retention.
- Decision: Continue Core/Security deploy API coverage on the host. Keep ARM-only; physical flashing remains disallowed by the both-architecture gate in root `AGENTS.md`.
- Commit: Not attempted; `.git` metadata is read-only in this environment and the files contain unrelated pre-existing edits.
- Artifacts/logs: `artifacts/logs/deploy-core-shareable-resume-vpcd-20261021.log`; `artifacts/logs/deploy-core-shareable-interface-resume-vpcd-20261021.log`; `artifacts/logs/deploy-security-keybuilder-persistent-rows-vpcd-20261021.log`.

## 2026-10-21 — Handoff: OwnerPIN virtual dispatch and Core API cutoff

- Step/experiment: `ownerpin-core-api-handoff`
- Change: Recorded the current dispatch fix, official Core client cutoff,
  focused VPCD results, build/flash constraints, and continuation steps in
  `.agent/HANDOFF.md`; added the reusable dispatch finding to `.agent/QKB.md`.
- Validation: Documentation checkpoint only. No new build, test, USB/PCSC,
  or hardware command was run.
- Result: Handoff records that temporary virtual-dispatch traces remain and
  normal host plus ARM builds are still required. No throughput metric was
  measured.
- Decision: Stop as requested. Resume with trace cleanup and normal host/ARM
  builds, then rerun focused VPCD probes and isolate the official client
  `APDUException.IO_ERROR` cutoff. Do not flash under the active ARM-only and
  both-architecture flash-gate conflict.
- Commit: Not attempted; prior repository sessions report `.git` metadata
  mounted read-only. Preserve the existing dirty worktree.
- Artifacts/logs: See `.agent/HANDOFF.md`; latest official Core result and raw
  APDUException logs are under `artifacts/results/deploy/core-vpcd-official-fixed/`
  and `artifacts/logs/deploy-core-apduexception-*-vpcd-20261021.log`.

## 2026-10-21 — OwnerPIN dispatch cleanup and deploy Core VPCD validation

- Step/experiment: `ownerpin-dispatch-cleanup-core-vpcd`
- Change: Removed temporary `INVOKEVIRTUAL_REF` / `INVOKEVIRTUAL_RESOLVE`
  trace calls from `bc_invoke.cpp`, preserving the inverse-mapped dynamic
  override fix. Updated TODO, QKB, and handoff with completed Core results and
  the corrected PC/SC timeout diagnosis.
- Validation: `./tools/build-pc` passed with `ROMMASK:PASS`; `./tools/build
  arm` passed (`build/arm/pico2_choupi.uf2`, image end `0x100c63e0`). No RISC-V
  build or flash. Focused host VPCD Core APDUs passed 11/11; six persistent
  Security DES/AES KeyBuilder rows passed 11/11; Multi AIDs ending `41` and
  `42` each selected in separate VPCD sessions (11/11). Official Core client
  completed 291-call summary with 0 wrong results; 14 official APDUException
  rows passed including `IO_ERROR` (`0x0004`).
- Result: The prior apparent `IO_ERROR` disconnect was a 900-second full-suite
  timeout; the longer run completed. The official CSV has 291 detail rows:
  272 OK, 11 allowed unsupported, and 8 `APPLET_NOT_INSTALLED`, matching the
  291-call client summary. Multi and Ext scenarios cannot run with the
  single-applet host VPCD setup. Host VPCD evidence only; no current-source
  hardware check.
- Decision: Keep the source fix. Next improve host Multi/Ext scenario setup and
  continue deploy Security/Crypto API coverage. Do not
  flash while the ARM-only constraint conflicts with the both-architecture
  gate.
- Commit: Not attempted; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/core-vpcd-official-full-20261021/`;
  `artifacts/results/deploy/core-apduexception-official-20261021/`;
  `artifacts/logs/deploy-core-ownerpin-dispatch-regression-20261021.log`;
  `artifacts/logs/deploy-security-keybuilder-dispatch-regression-20261021.log`;
  `artifacts/logs/deploy-multi-a-vpcd-current-20261021.log`;
  `artifacts/logs/deploy-multi-b-vpcd-current-20261021.log`.
## 2026-10-21 — Single physical Pico Security API coverage

- Step/experiment: `single-physical-pico-security-api-coverage`
- Change: Recorded the reader-index diagnosis, physical Security API results,
  and current-source validation limitation in TODO, QKB, and handoff.
- Validation: `pcsc_scan -r` identified the physical Pico as the third PC/SC
  terminal after two empty Virtual PCD readers. Read-only `gp --info` and
  `gp --list` succeeded. `AlgTest_JavaCard/deploy/run_api_test.sh --areas
  Security --api 305 --reader-index 3` completed upload, 901 calls, and CAP
  cleanup successfully. No source build or flash was run.
- Result: 822 `UNSUPPORTED_PER_SPEC`, 43 `OK`, 15 `SW_ERROR`, 13
  `EXC_RUNTIME`, 8 `WRONG_RESULT`; the tests ran against the existing physical
  firmware, not the latest unflashed source. The first attempt used reader
  index 1, stalled on an empty Virtual PCD terminal, and was interrupted; its
  test CAP was explicitly removed before retry.
- Decision: Keep the complete physical CSV as a baseline. Compare failures
  against current-source VPCD probes before changing implementation; continue
  with Core/JCSystem priorities. Do not flash while ARM-only and the
  both-architecture gate conflict.
- Commit: Not created; repository `.git` metadata is mounted read-only in this
  environment. Existing worktree changes were preserved.
- Artifacts/logs: `artifacts/results/deploy/pico2-single-security-reader3-20261021.csv`
  (SHA-256 `49032c715eace1fe830e3c42da83603d9f1db83b51deda100c37b6556af86bee`);
  interrupted run `artifacts/results/deploy/pico2-single-security-20261021.csv`.

## 2026-10-21 — Single physical Pico Crypto API coverage

- Step/experiment: `single-physical-pico-crypto-api-coverage`
- Change: Added the physical Crypto 3.0.5 results and follow-up classification
  to the TODO and handoff. No implementation source changed.
- Validation: On the explicitly named Pico reader (client terminal index 3),
  `AlgTest_JavaCard/deploy/run_api_test.sh --areas Crypto --api 305` completed
  upload, 466 client calls, and CAP cleanup. Excluded the documented RSA-2048
  key-pair reader-drop call. No build or flash was run.
- Result: 444 `UNSUPPORTED_PER_SPEC`, 14 `OK`, six `EXC_CARDRUNTIME`, and two
  `SW_ERROR`; both test upload and cleanup stages were `OK`. `6F00` occurred
  at the Signature and Cipher constructors. ILLEGAL_VALUE (`0x0002`) occurred
  in six Cipher test cases. These are observations from the firmware already
  on the board and not current-source verification.
- Decision: Compare the 8 non-unsupported results with current-source VPCD
  before making code changes. Continue to `JavacardxExt` on the same physical
  reader.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/pico2-single-crypto-reader3-20261021.csv`
  (SHA-256 `1a9766f365ae0d47c370d884017779c2f1ad2f4c2d8baec9120f0b38a7aa4ccf`).

## 2026-10-21 — Wide static-field tags for JavaCardX CAP initialization

- Step/experiment: `javacardx-wide-static-tags`
- Change: Added a 16-bit big-endian static-field root tag for offsets above
  255, retaining the three-byte legacy tag for lower offsets. Removed the
  install-time one-byte offset rejection while keeping CAP image bounds checks.
  The JavaCardX 3.0.5 CAP has 164 static reference slots and initializer
  offsets through 326.
- Validation: `./tools/build arm` passed. `./tools/build-pc` passed with
  pre-existing Rust and packed-member compiler warnings. Current-source VPCD
  install and SELECT passed: `TEST:SUMMARY pass=11 fail=0`; SELECT returned
  `9000`. RISC-V build was skipped under the active ARM-only constraint; no
  physical card was flashed or modified because the flash gate requires both
  architecture builds.
- Formatting: Root scoped `git diff --check` passed. The nested CHOUPI VM diff
  check reports CRLF line endings across its already modified source files;
  those files were not normalized to avoid broad unrelated diffs.
- Result: JavaCardX no longer fails CAP INSTALL because static initializer
  offsets exceed one byte. Physical firmware validation remains pending.
- Decision: Continue with the JavaCardX API call results after current-source
  firmware can be tested on a physical reader.
- Commit: Not created; repository and nested `.git` metadata are read-only.
- Artifacts/logs: `artifacts/results/deploy/javacardx-vpcd-select-only-wide-static-20261021.log`
  (SHA-256 `57a7070ce1451dbee1458d4fbd51167389c7c4929a98b27049f44eb68026fb40`).

## 2026-10-21 — Reboot BOOTSEL boards and enable multi-reader PC/SC access

- Step/experiment: `three-pico-pcsc-reenumeration`
- Change: Rebooted BOOTSEL boards to their existing applications without
  flashing. Updated `tools/pico2-pcsc-reader-root` to grant pcscd ACLs to one to
  three detected CHOUPI readers and restart pcscd once.
- Validation: USB enumeration found CHOUPI serials `D92E743D5F640C0F` and
  `E0E14525E64AF6ED`, plus generic Pico serial `5CFC5328F5BDE437`. Both
  CHOUPI descriptors include CCID; the generic Pico descriptor has only CDC
  and a vendor reset interface. `pcsc_scan -r` lists only the original CHOUPI
  because the installed helper ACLs one USB node and rejects multiple devices.
  `bash -n tools/pico2-pcsc-reader-root tools/install-pcsc-sudoers` passed;
  `/usr/sbin/visudo` is available. Updated installer has not been run yet.
- Result: Two boards are running CHOUPI and one board runs generic Pico
  firmware. Parallel tests are not ready until the revised helper is installed
  and PC/SC confirms both CCID readers. No device was flashed.
- Decision: User should run `sudo ./tools/install-pcsc-sudoers`, then the
  approved `sudo -n /usr/local/sbin/pico2-pcsc-reader`; resume reader validation
  and two-worker testing afterward. Third board requires CHOUPI firmware.
- Commit: Not created; repository `.git` metadata is read-only.
- Artifacts/logs: Descriptor and enumeration output inspected live; no log file.

## 2026-10-21 — ROM package visibility and imported version compatibility

- Step/experiment: `vpcd-rom-package-import-lookup`
- Change: Import resolution now accepts a resident package when AID and major
  version match and the imported minor version is no newer than the resident
  minor version. PC VPCD package-existence checks now include CAPs exposed by
  the built-in ROM-mask callback, which are stored outside the mutable package
  bitmap.
- Validation: `./tools/build arm` and `./tools/build-pc` passed. Final current-
  source Core VPCD install/SELECT returned `TEST:SUMMARY pass=11 fail=0` at
  `artifacts/results/deploy/core-import-fix-final-select-vpcd-20261021.log`.
  The JXExt APDU `B0835102010000` still returns `6F00`: its catch reference
  imports `javacardx.external` AID `A0000000620203` version 1.0, absent from the
  generated ROM mask. That unresolved API dependency is now in `TODO.md` and
  `.agent/QKB.md`. The physical-device scan found one active reader and two
  BOOTSEL devices; no CAP was uploaded and no firmware was flashed. RISC-V was
  intentionally not built under the ARM-only constraint.
- Result: The host emulator can now look up built-in ROM-mask packages and
  compatible package versions. A missing API package still produces a runtime
  failure after successful install/select and needs install-time import
  validation or an expanded ROM profile.
- Decision: Next, implement CAP import validation at INSTALL, then decide from
  the supported JCVM profile whether to include `javacardx.external`.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: Core result above; JXExt trace at
  `artifacts/results/deploy/javacardxext-import-header-trace-vpcd-20261021.log`.

## 2026-10-21 � Three Pico 2 USB and PC/SC enumeration

- Step/experiment: `three-pico-live-enumeration`
- Change: Recorded current physical-device state and refreshed PC/SC access;
  no source or firmware changes.
- Validation: Elevated `lsusb -d 2e8a:` found one `2e8a:0009` CHOUPI device
  and two `2e8a:000f` RP2350 BOOTSEL devices. The approved PC/SC access helper
  succeeded. Elevated `pcsc_scan -r` found one physical Pico reader plus two
  empty VPCD readers. Elevated `PICO2_HOST_USB_ACCESS=1
  ./tools/run-deploy-parallel --dry-run --min-devices 1` correctly routed the
  one physical Pico to a worker; no card writes occurred.
- Result: The two newly inserted BOOTSEL boards are visible to the VM but
  cannot yet serve as PC/SC cards. Parallel orchestration is ready for three
  application-mode devices. No firmware was flashed.
- Decision: Resume the offline JCVM exception/package-resolution investigation;
  return to the physical parallel run when all three boards expose CHOUPI CCID.
- Commit: Not created; repository `.git` metadata is read-only.
- Artifacts/logs: `.agent/HANDOFF.md`; dry-run output was inspected directly.

## 2026-10-21 — Three-reader physical API test setup

- Step/experiment: `parallel-pico-deploy-runner`
- Change: Added exact `--reader-name` selection to the deploy API runner,
  SmartcardIO terminal listing to the Java index helper, and
  `tools/run-deploy-parallel` to distribute API areas over up to three cards
  with separate CSV and log outputs. The runner defaults to requiring all
  three physical PC/SC Pico readers and excludes the known RSA-2048 reader-drop
  case.
- Validation: Python syntax and scoped `git diff --check` passed. The
  non-mutating dry run resolved the active Pico as Java terminal 3 despite two
  empty virtual readers. USB enumeration showed one application-mode Pico
  (`2e8a:0009`) and two BOOTSEL devices (`2e8a:000f`). A normal invocation
  safely stopped before starting workers: found one live PC/SC Pico, required
  three. No card CAP was uploaded by this validation.
- Result: Three-way orchestration is ready once all devices run CHOUPI and
  appear as unique PC/SC readers. BOOTSEL devices are deliberately not counted.
- Decision: Start three independent area workers when the two BOOTSEL devices
  return to application mode; preserve each worker's separate result file.
- Commit: Not created; repository `.git` metadata is read-only.
- Artifacts/logs: Runner `tools/run-deploy-parallel`; exact-name API selection
  in `AlgTest_JavaCard/deploy/api_card_runner.py` and `tools/JCAlgTestReaderIndex.java`.


## 2026-10-21 — Reconcile three Pico USB states

- Step/experiment: `three-board-usb-state-check`
- Change: Recorded live board/PCSC findings and updated the physical-test handoff and QKB with the stale-helper and BOOTSEL-passthrough recovery steps.
- Validation: Elevated `lsusb -d 2e8a:` found three `2e8a:0009` boards; elevated `picotool info` found no BOOTSEL targets and identified all three as application-mode RP2350s. Targeted `picotool reboot --usb --force --ser E0E14525E64AF6ED` failed with `Unable to locate reset interface on the device`. Elevated `pcsc_scan -r` listed only the original Pico. Elevated `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --min-devices 1 --max-devices 3 --dry-run` resolved the physical reader and assigned all areas without card writes.
- Result: The added boards are not currently available in BOOTSEL to this VM. The updated multi-device ACL helper is not installed; the installed single-device helper rejects three matching Pico USB IDs. No firmware was flashed.
- Decision: Resolve VirtualBox BOOTSEL passthrough and install the updated helper before attempting parallel physical API tests. Continue offline while those prerequisites are unavailable.
- Commit: Pending; worktree contains unrelated pre-existing changes, so stage only this step's documentation additions if committing.
- Artifacts/logs: `.agent/HANDOFF.md`, `.agent/QKB.md`; command output inspected directly.

## 2026-10-21 — Serial-targeted multi-Pico flashing and startup

- Step/experiment: `serial-targeted-pico-cycle`
- Change: Added `--serial SERIAL` selection to `tools/flash`, `tools/monitor`, and `tools/cycle`. The flash wrapper now puts picotool selectors before positional arguments, targets CDC BOOTSEL recovery by stable by-id path, and requests picotool flash verification.
- Validation: `bash -n tools/flash tools/monitor tools/cycle`, CLI help, and scoped `git diff --check` passed. `./tools/build arm` and `./tools/build riscv` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial 5CFC5328F5BDE437` passed with bounded 18-second serial output. A second cycle targeting `E0E14525E64AF6ED` recovered from `Unable to locate reset interface` through CDC 1200-baud BOOTSEL in 2 seconds, flashed and verified at 100%, and passed runtime output. Latest `lsusb -d 2e8a:` shows all three board serials enumerating as `2e8a:0009`.
- Result: All three boards run CHOUPI firmware. PC/SC still exposes only the original reader because the installed helper rejects three devices (`found 3`). The repository helper handles up to three; operator install is required before card tests. The parallel runner dry run safely reports two missing PC/SC readers; no API worker or APDU test was started.
- Decision: Operator should run `sudo ./tools/install-pcsc-sudoers`, then `sudo -n /usr/local/sbin/pico2-pcsc-reader`, and confirm three Pico readers with `pcsc_scan -r`. Then run the three-worker API suite.
- Commit: Not created; `git apply --cached` failed because `.git/index.lock` cannot be created (`Read-only file system`). No files were staged; unrelated worktree changes were preserved.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261021T181442Z.log` SHA-256 `3e4d09fbee4fcac3f60e20259607be07bf89f001de8c4f373fdebd4a7257715b`; `artifacts/logs/cycle-choupi-20261021T181723Z.log` SHA-256 `035f2437d7fff62d1ea2e31ffbc85a82e4877440cdf20abd590110b9fc0e119d`; ARM UF2 SHA-256 `ffea2e2153fa9270877d5b6417fd1287984227b169d95684aa88ce96a0f3c320`.

## 2026-10-21 — Three-reader deploy coverage and scenario rollback

- Step/experiment: `three-reader-deploy-api-305`
- Change: Ran the deploy API areas concurrently on all three named Pico readers. Fixed the Core scenario CAP path to roll back a package after GPPro reports `CAP loaded` followed by INSTALL refusal; ordinary API CAP uploads use the same helper. Removed the Ext package left by the first scenario run.
- Validation: Elevated PC/SC showed three CHOUPI readers (D92, E0E, 5CFC) plus two empty Virtual PCD terminals. The three parallel workers completed; runner exit was 1 because optional CAP imports were unavailable. Security: 901 calls (820 allowed unsupported, 43 OK, 15 SW_ERROR, 13 EXC_RUNTIME, 8 WRONG_RESULT, 2 skipped). Crypto: 466 calls (434 allowed unsupported, 11 OK, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 2 SW_ERROR). Service: 124 calls (94 OK, 28 WRONG_RESULT, 1 SW_ERROR, 1 EXC_RUNTIME). Core: 291 calls (272 OK, 15 allowed unsupported, 4 Ext scenario calls not run). JavacardxSec/Javacardx/JavacardxExt/JavacardxInt/Biometry INSTALLs returned 6A88. Focused Core reruns verified `UPLOAD_EXT_ROLLBACK OK` after 6A88 and GPPro then showed only the ISD; each rerun reproduced one `Applet.deselect()` WRONG_RESULT (`returned=-26`). Python syntax and scoped diff checks passed. No firmware was flashed; hardware results use images already on the boards.
- Result: Three-card area routing works and every observed partial install was removed after the scenario-path fix. Optional API imports still limit coverage, and the repeated `Applet.deselect()` mismatch needs diagnosis. The many unsupported algorithm results are expected and were not treated as defects.
- Decision: Preserve the deploy artifacts, investigate the isolated Core mismatch, and continue with missing API package/runtime support. The ARM-only constraint remains in force; no RISC-V build or flash was attempted.
- Commit: Not created; repository `.git` metadata is read-only in this execution environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-204343/` (pico1 CSV SHA-256 `c83f55b46215d40393b40aeb0f3960b4821f6461ef830c23dc8b35cbe41c4bf7`; pico2 `5204d26c278afbe2c843b590613c728dc3190af6d865e244b6e7ea84d47e29ec`; pico3 `18a8ef65b41deef44355fb8c62b2a4d606087a19f6cf81e9029166aef7a215b6`); focused Core CSVs `artifacts/results/deploy/pico3-core-scenario-rollback-20261021.csv` and `artifacts/results/deploy/pico3-core-repeat-20261021.csv`.

## 2026-10-21 — Clean CAP registry after failed install transaction

- Step/experiment: `cap-install-commit-failure-metadata-rollback`
- Change: If the persistent filesystem transaction fails its final commit sync after the CAP slot metadata has registered the applet, attempt to remove that instance record and sync the metadata store. Added the existing `path_static16` ABI to the persistent-transaction test stub so that this regression suite compiles against current source.
- Validation: `./tools/build arm` passed; `./tools/build-pc` passed (with existing Rust and packed-member warnings). `./tools/test-rp2350-fs-overlay` and `./tools/test-persistent-transaction` passed with `TEST:SUMMARY fail=0`; Python syntax and scoped `git diff --check` passed. These tests do not inject a commit failure through the full `CapLoadSession` install path, so the CAP metadata rollback branch remains unverified end to end. No hardware flash was performed.
- Result: Source compiles for ARM and host; filesystem rollback and flash-overlay regressions pass. The commit-failure metadata cleanup is best-effort and still needs full-path fault injection.
- Decision: Add targeted install-path fault injection before considering this atomicity issue resolved. Continue ARM-only; no RISC-V build.
- Commit: Not created; repository `.git` metadata is read-only in this execution environment.
- Artifacts/logs: Build outputs under `build/arm/` and `build/choupi-pc/`; test binaries under `build/tests/rp2350-fs-overlay/` and `build/tests/persistent-transaction/`.

## 2026-10-21 — Reset VM state on CCID power-on

- Step/experiment: `ccid-poweron-reset-lifecycle`
- Change: `PC_to_RDR_IccPowerOn` now drops selected applet state, clears `CLEAR_ON_RESET` arrays, resets SCP state, and aborts a partial CAP transfer before restoring installed package metadata. USB bus reset also clears selected applet and reset-transient state. This makes the physical CCID path apply reset semantics already used by the host VPCD bridge.
- Validation: `./tools/build arm` passed; `./tools/build-pc` passed with existing upstream Rust warnings; `./tools/test-pc-vpcd-applet --passes 1 --log artifacts/results/deploy/vpcd-reset-lifecycle-20261021.log` passed (`TEST:SUMMARY pass=11 fail=0`). Initial sandbox invocation failed to bind its loopback listener with `Operation not permitted`; the same host-only test passed via the elevated path. The physical three-reader Core/Security/Crypto deploy pass preceded this source change and does not validate the new firmware behavior. RISC-V build and flashing skipped under the ARM-only constraint and two-architecture flash gate.
- Result: The focused physical Core run again found one `Applet.deselect()` counter mismatch: the API probe recorded 239 select callbacks and 98 deselect callbacks. Client disassembly confirms this is the applet's callback counter, not a VM return value. The reset-path omission is fixed, but the mismatch's complete cause is not yet established.
- Decision: Keep the reset handling. Rebuild both architectures before the next flashable hardware retest; continue investigating callback-count divergence and test physical power/reset behavior.
- Commit: Not created; `.git` metadata is read-only in this execution context.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-210341/pico1.csv` SHA-256 `e94c025620f16804d9c156439a1105d7d327f8d39c39f2918e744fbd51858f4a`; `pico2.csv` SHA-256 `9be757173da3c818afe51bc8bcbce9b32ab8f737ec56695ed2211c0addf76bbc`; `pico3.csv` SHA-256 `be87385da3c15c0cd51928588e3573666955cdb839d4f00a2b358d226ff761b3`; VPCD log SHA-256 `f97674f923f105e8d1b858e29554f1ac49a90bf65d325ebd9403e674ae104a6d`.

## 2026-10-21 — Physically verify CCID reset lifecycle

- Step/experiment: `pico3-core-reset-lifecycle-retest`
- Change: Flashed the reset-state fix to Pico 3 (serial `5CFC5328F5BDE437`) after ARM and RISC-V builds passed. Refreshed pcscd ACLs after re-enumeration so all three physical readers were visible again.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial 5CFC5328F5BDE437` passed flash verification and its 18-second serial health check (`TEST:SUMMARY pass=1 fail=0`; no `FAULT`/`TEST:FAIL`). Two exact-reader official Core 3.0.5 install/test/cleanup runs passed, each with 291 calls: 272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext calls `APPLET_NOT_INSTALLED`; no `WRONG_RESULT`, `SW_ERROR`, or `EXC_*`. Both runs reported `Applet.select()` and `Applet.deselect()` counters as `-19`, and both removed Core/Multi CAPs. `git diff --check` and Python syntax checks passed.
- Result: The missing CCID power-on reset handling was the cause of the fluctuating Core callback-counter mismatch. Applying reset semantics cleared the mismatch on two consecutive physical runs. The Ext CAP still fails INSTALL with `6A88` due to its missing import; partial package rollback passes. A `WATCHDOG_RECOVERY cause=other` marker appeared once in the serial cycle log alongside repeated passing runtime summaries; the marker alone does not indicate a reset or test failure.
- Decision: Mark the Core callback-count issue fixed. Continue with remaining API/runtime priorities and optional package dependencies; the three-reader setup is usable for parallel runs.
- Commit: Not created; `.git` metadata is read-only in this execution context.
- Artifacts/logs: Core CSVs `artifacts/results/deploy/pico3-core-resetfix-20261021.csv` SHA-256 `e536e17f8785f79d327af4cfcb890d2de0fab1bbd34a1b482642e6943f7b060e` and `artifacts/results/deploy/pico3-core-resetfix-repeat-20261021.csv` SHA-256 `d6f1e16a7e7442a39358c19833a35334d9f4ae8fe61fe3df81f4115ffc337b1f`; cycle log `artifacts/logs/cycle-choupi-20261021T191200Z.log` SHA-256 `34d8f780acec083c15c93aa3279162973911dc366055e373d684841688c1a54e`.

## 2026-10-21 — Synchronize three Pico images and repeat API 3.0.5 sweep

- Step/experiment: `three-pico-current-image-api305`
- Change: Flashed the validated ARM image to Pico 1 and Pico 2 by serial so all three physical boards ran identical firmware. Refreshed pcscd ACLs for all three USB nodes and reran the complete parallel API 3.0.5 deployment matrix.
- Validation: Both architecture builds passed before the cycles. Pico 1 (`D92E743D5F640C0F`) and Pico 2 (`E0E14525E64AF6ED`) each passed flash verification and an 18-second serial runtime capture. The parallel runner completed all workers. Pico 3 Core: 291 calls, 272 OK, 15 allowed unsupported, four optional Ext calls not installed, zero wrong/status/runtime errors; its JCSystem calls were 37 OK and one spec-allowed unsupported. Core/Multi, Crypto, and Service cleanup succeeded. JavacardxSec/Javacardx/JavacardxExt/JavacardxInt/Biometry CAP INSTALLs were refused with `6A88` for missing imports and rolled back. Security: 901 calls (63 OK, 820 allowed unsupported, 13 EXC_RUNTIME, 2 WRONG_RESULT, 1 SW_ERROR, 2 skipped). Crypto: 466 calls (105 OK, 434 allowed unsupported, 17 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 3 SW_ERROR, 28 WRONG_RESULT). Service: 124 calls (94 OK, 28 WRONG_RESULT, 1 SW_ERROR, 1 EXC_RUNTIME). Those non-Core API findings are recorded; cryptographic algorithm absences remain expected and were not implemented.
- Result: Three-way physical deploy is repeatable on matched current images. The Core/JCSystem behavior and physical package cleanup pass. Optional API imports remain absent, and Security/Crypto/Service have separate API discrepancies outside this Core-focused step.
- Decision: Keep the three-reader setup for future independent API coverage. Continue with the remaining VM/runtime priorities while preserving the user's scope of correct applet and JCSystem execution.
- Commit: Not created; `.git` metadata is read-only in this execution context.
- Artifacts/logs: CSVs under `artifacts/results/deploy/parallel-20261021-211910/`; hashes Pico 1 `a7565906e8618c8bb164e406b8af2a99856d0aa392b83b11a95d5e3f8cb4cc91`, Pico 2 `da135c19e1de741037e69dc45453766846e28a33fa4796aa31657de040e46cb7`, Pico 3 `22a393d6c1d4643af35240b6eccf71167abc23c4447515401f4da470eae50a1d`. Pico 1/2 cycle logs `artifacts/logs/cycle-choupi-20261021T191640Z.log` SHA-256 `0afa2a93e03f9d21f3d412c8a8ead6371c0fd66b02a85c1941413c77f6a0ea88` and `artifacts/logs/cycle-choupi-20261021T191751Z.log` SHA-256 `2083967473d6000ca13b0bc982b1f11886def36ca89de7ffc6ef960fac74426e`.

## 2026-10-21 — Verify JCAlgTest power-up restoration on Pico 1

- Step/experiment: `jcalgtest-powerup-restoration-pico1`
- Change: Installed the repository-built JCAlgTest 1.8.2 fixture on Pico 1 after confirming the deploy suite had cleaned its own unrelated CAPs. The fixture was installed with GlobalPlatformPro from `build/choupi-algtest/output/JCAlgTest-1.8.2-rebuilt.cap` (SHA-256 `1b2b17f5999e070f14d184df9b83d7f11abcdbcd5ee7f2e30205d5a324941d57`).
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup 'Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F) 00 00'` passed registry lookup, applet SELECT (`9000`), GETVERSION (`1.8.2_jc305`), and JCSystem info after separate PC/SC sessions/power-on. Earlier attempts on Pico 3 and Pico 1 before fixture installation showed only the ISD and sent no applet APDU. Pico 3 later remained present in `lsusb` but absent from `pcsc_scan -r` despite ACL refresh and valid CCID descriptors.
- Result: The installed JCAlgTest package/applet restores and runs through a PC/SC power-up transition on Pico 1. The test fixture remains installed there. Pico 3 reader visibility is an outstanding host/PCSC issue; no persistence conclusion was drawn for that card.
- Decision: Retain Pico 1 as the persistence test fixture; recover Pico 3 PC/SC visibility before assigning more tests to it.
- Commit: Not created; `.git` metadata is read-only in this execution context.
- Artifacts/logs: Passing log `artifacts/logs/pico-powerup-persistence-20261021T192510Z.log` SHA-256 `7452e82a0eac0c5c8f769d2c0eadba0c9a5a0a422abef645e436bb99586cc573`. Failed no-fixture registry checks: Pico 3 `...T192225Z.log` SHA-256 `691d823588fbefdaa5a21397e18afc24383d9b3a07d84d48c7378ea2e740ede9`; Pico 1 `...T192259Z.log` SHA-256 `7fce4bdec24d5495dab8a395fa1f63ae56478b3db6f5f5b1bbb16ff59b468bcc`.

## 2026-10-21 — Refresh three-reader access and probe parallel client waits

- Step/experiment: `three-reader-parallel-wait-reproduction`
- Change: After cycling Pico 3 to the current ARM image, refreshed the approved PC/SC ACL helper and verified the parallel assignment on all three named physical readers. Documented an incomplete second parallel API run and its affected card/package state in `.agent/HANDOFF.md` and `.agent/QKB.md`.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial 5CFC5328F5BDE437` passed flash verification and finite startup capture. The first `pcsc_scan -r` showed only two readers; `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported `count=3`, and the following scan listed serials D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305` started all three workers. Pico 2 Crypto completed (466 calls) and cleaned up; Pico 3 Core completed (291 calls) and cleaned up; optional package INSTALL refusals returned `6A88` with successful rollback. Pico 1 Security and Pico 2 Service stalled in the Java client, and Pico 3 Javacardx upload stalled. Interrupted coordinator after roughly three minutes; no worker processes remained. No build was needed because source was unchanged; `git diff --check` was run on the documentation edits.
- Result: All three readers are accessible after the approved helper refresh. A full parallel run had already completed at `parallel-20261021-211910/`; this reproduction is incomplete and indicates an intermittent concurrent client/transport wait, not a VM result. Preserve the partial artifacts and do not treat them as completed coverage.
- Decision: Inspect affected readers' package registries and clean partial installs, then retry Security/Service/Javacardx sequentially on exact readers before attempting another parallel matrix. Capture per-stage client output to determine whether reader selection or transport caused the wait.
- Commit: Not created; `.git` metadata is read-only in this execution environment.
- Artifacts/logs: Pico 3 cycle `artifacts/logs/cycle-choupi-20261021T193649Z.log` SHA-256 `782ecbf1402a9b7cb7907e347784d15adbcaa32a3417ef210ff2f84a21b66a34`; incomplete results in `artifacts/results/deploy/parallel-20261021-213747/`. CSV hashes Pico 1 `682526db235a72a7b704b035768c54d1932ab0ebdb88fcd3fad364bf41db40b3`, Pico 2 `13b09a4627fbf6540dd94b46741fd6f19cdd137224283056c5f1675075153887`, Pico 3 `8b185c436cf00c7bddd744649f647491acafa7f5bafde369c5ffec0ad7ccfc5d`.

## 2026-10-21 — Re-run stalled API areas sequentially

- Step/experiment: `sequential-security-service-retest`
- Change: Inspected card registries after the interrupted three-worker run and removed only orphaned Security/Service test CAP packages. Kept the JCAlgTest fixture on Pico 1; Pico 3's registry was already clean.
- Validation: Security on Pico 1, using exact PC/SC reader selection, completed 901 calls in about two minutes and runner cleanup passed: 822 `UNSUPPORTED_PER_SPEC`, 63 `OK`, 13 `EXC_RUNTIME`, 2 `WRONG_RESULT`, 1 `SW_ERROR`. Service on Pico 2 completed 124 calls in about 12 seconds and cleanup passed: 94 `OK`, 28 `WRONG_RESULT`, 1 `SW_ERROR`, 1 `EXC_RUNTIME`. These runs used the elevated USB/PCSC host-access path. `git diff --check` passed for the continuity edits. No source changed, so no builds were run.
- Result: Both previously stalled areas complete when run alone and reproduce their earlier API outcomes. This points toward a possible concurrency/host-scheduling interaction, but does not prove its cause. The full matrix at `parallel-20261021-211910/` remains the complete parallel evidence.
- Decision: Next test two readers with one short area each and timestamped stages; only try three-way concurrency again if two readers finish reliably. Do not treat method-level Security/Service discrepancies as part of the Core/JCSystem implementation scope.
- Commit: Not created; `.git` metadata is read-only in this execution environment.
- Artifacts/logs: Pico 1 CSV `artifacts/results/deploy/pico1-security-sequential-20261021.csv` SHA-256 `9d33c4e46a62f3578c846012ffae44260474b1c46d5ef88a6b5b9772618d8759`; Pico 2 CSV `artifacts/results/deploy/pico2-service-sequential-20261021.csv` SHA-256 `11f30e8e62522c13b5cc8a0de07ff5d820232be877fb918fed0d85f935d5c162`.

## 2026-10-21 — Validate one-area parallel batches on three Picos

- Step/experiment: `three-reader-one-area-batch`
- Change: Exercised the multi-reader runner with one independent area per worker after the earlier full multi-area run stalled. No source changes.
- Validation: Two-reader `Security,Service` batch completed both workers with exit 0 and cleanup. Three-reader `Crypto,Core,Service` batch completed all workers: Crypto 466 calls/cleanup, Core 291 calls and Core/Multi cleanup, Service 124 calls/cleanup. Optional Core Ext INSTALL was refused with `6A88` and its partial package rollback passed; the coordinator exit was 1 because that optional stage is recorded as failed. Final elevated `pcsc_scan -r` showed all three physical Pico serials. `git diff --check` passed for continuity files. No builds needed.
- Result: Parallel use of three readers is confirmed for one area per card. The earlier incomplete run was a chained multi-area worker case; this data does not establish which later area stalled. Core applet/JCSystem checks remained 272 OK, 15 spec-allowed unsupported, and four optional Ext methods unavailable; no new algorithm support was added.
- Decision: Keep future parallel batches to one area per physical reader until the multi-area chaining wait is isolated. Treat missing imports/`6A88` and spec-allowed unsupported calls as expected; investigate only true applet/runtime correctness issues in scope.
- Commit: Not created; `.git` metadata is read-only in this execution environment.
- Artifacts/logs: Two-reader run `artifacts/results/deploy/parallel-20261021-214449/`; three-reader run `artifacts/results/deploy/parallel-20261021-214712/`. Three-reader CSV hashes: Pico 1 `a8de9f60a92f743c68fa6d83f6b40e5a589baa00df5bc1b8605f8be29369b471`, Pico 2 `3f406b78e69cfabaaefb23b0f24b8e984a248ae31f8b5416185ac838d099cbcc`, Pico 3 `1fc8ec6747a67708929a43e9847f1f3024fa6600cd788e09fc0c099cb40aca3b`.
## 2026-10-21 — Run three-reader batch and fix stale import lookup

- Step/experiment: `three-reader-import-guard`
- Change: Refreshed PC/SC access for all three attached Picos and ran Core,
  Crypto, and Service as independent parallel areas. Fixed dynamic package
  lookup so a stale package bitmap bit cannot make an empty CAP slot appear
  resident. Added regression cases for stale/present/cleared dynamic package
  state and missing-import INSTALL stage 10.
- Validation: `sudo -n /usr/local/sbin/pico2-pcsc-reader` granted three nodes;
  elevated `pcsc_scan -r` listed D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437. Physical three-reader run exited 0 for all workers:
  Crypto 466 calls, Core 291 calls (272 OK, 15 spec-allowed unsupported; Ext
  refused with expected `6A88` and rollback passed), Service 124 calls.
  `./tools/test-persistent-transaction`, `./tools/test-install-status`,
  `./tools/build arm`, `./tools/build-pc`, and host VPCD applet test passed;
  VPCD applet test summary was `pass=11 fail=0`. RISC-V was not built and no
  firmware was flashed. Physical validation of the source change was therefore
  intentionally skipped under the ARM-only build constraint and current
  two-architecture flash gate.
- Result: Three physical readers can run one independent deploy area each;
  this bounded shape completed with cleanup on every card. The missing Ext
  dependency still correctly rejects INSTALL with `6A88`. Stale-bitmap fix is
  covered offline but remains unverified on hardware. Core's 38 JCSystem rows
  were 37 `OK` and one profile-allowed `UNSUPPORTED_PER_SPEC` for
  `requestObjectDeletion()` (`ILLEGAL_USE`); transient/global arrays, memory
  forms, transactions, and AID/applet-state calls passed.
- Decision: Keep using one area per reader for parallel hardware testing.
  Continue with final-sync fault injection through CAP install; revisit
  physical validation only after the architecture flash gate is resolved.
- Commit: Not created; `.git` metadata/index are read-only in this execution
  environment, and the shared worktree contains unrelated changes that must
  not be included accidentally.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-current/`; CSV
  SHA-256 values: Pico 1
  `e46785b821c74f7962667288d06ace7b4083c0d669396a53b2e53f514333a5a7`, Pico 2
  `d2740d7bf1fe71a8a5e931d47853273c77e71081a2fc821f82e044fb0c71950e`, Pico 3
  `e5721881cc414d53afaf88c8419af7b6a99e87bb9af0b36be0765b4306f68cb3`.
  Host VPCD log `artifacts/logs/pc-vpcd-applet-import-fix-20261021.log`,
  SHA-256 `878a9f63bb2a99bcbb942ebaee5fa8d1ba0c909ee0a62b21221d9f01285b4b31`.


## 2026-10-21 — Protect CAP registry metadata from interrupted updates

- Step/experiment: `cap-registry-copy-on-write`
- Change: Added per-slot mirror metadata sectors in the unused flash gap. CAP
  registry updates now alternate version-4 headers between primary and mirror,
  with CRC validation and generation selection during initialization. Existing
  version-3 primary headers remain accepted. Extended the RP2350 filesystem
  overlay test to inject power cuts in both alternating update directions.
- Validation: `./tools/test-rp2350-fs-overlay` passed all checks, including
  interrupted registry update recovery and 1,000 randomized filesystem power
  cuts (`TEST:SUMMARY fail=0`); an explicit v3 header fixture remains readable.
  `./tools/build arm`, `./tools/build-pc`, and the host VPCD JCAlgTest 1.8.2
  regression passed (`TEST:SUMMARY pass=11 fail=0`). The overlay host test
  uses `mallinfo2()` on glibc to avoid the previous deprecation warning; host
  build reports existing Rust warnings. No RISC-V build or physical flash was
  run.
- Result: A power cut while rewriting one CAP metadata copy leaves the previous
  valid copy available and retains CAP code. This does not make the separate
  CAP header and filesystem snapshot one atomic transaction; a cut between
  publishing an applet instance and final filesystem commit remains open.
- Decision: Add durable cross-store install journaling and full CAP-install
  failure injection next. Keep hardware validation pending because the current
  ARM-only constraint conflicts with the repository's two-architecture flash
  gate.
- Commit: Not created; the shared worktree has extensive unrelated changes,
  and `.git` metadata/index are read-only in this environment.
- Artifacts/logs: `./tools/test-rp2350-fs-overlay` output; VPCD log
  `artifacts/logs/pc-vpcd-cap-metadata-cow-20261021.log`, SHA-256
  `178a83909da7c77e6dc352fde509ff953139357b4807c8f4d6641b3788beeee6`.

## 2026-10-21 — Add durable CAP install intent and boot replay

- Step/experiment: `cap-install-recovery-journal`
- Change: Added transaction-aware opaque persistent-record APIs. CAP INSTALL now
  commits a CRC-protected intent (package/applet/instance AIDs and class index)
  together with applet persistent state, publishes the COW CAP registry entry
  afterward, then removes the intent in a second transaction. On card power-on,
  `restoreInstalled()` validates pending intents, publishes missing registry
  entries idempotently, and transactionally clears completed intents.
- Validation: `./tools/test-persistent-transaction` passed with five journal
  record cases, including rollback, readback, failed-sync preservation, erase
  rollback, and cleanup. `./tools/build arm` and `./tools/build-pc` passed. A
  fresh VPCD JCAlgTest 1.8.2 install reached
  `VPCD:APPLET_INSTALLED class-index=148`; `TEST:SUMMARY pass=11 fail=0`.
  Host build retains existing Rust/C++ warnings. No RISC-V build or hardware
  flash was run.
- Result: The durable intent now precedes the separate CAP registry update, so
  boot has enough validated information to complete an interrupted publication
  without duplicate registration. Full injected restart tests at each boundary
  are still missing; atomicity is not yet declared complete.
- Decision: Add deterministic failure injection for registry publication and
  journal cleanup, restart VPCD from the saved image, and verify successful
  recovery/SELECT. Preserve the current ARM-only scope and do not bypass the
  repository's two-architecture flash gate.
- Commit: Not created; the shared worktree has extensive unrelated edits and
  `.git` metadata/index are read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/install-journal-vpcd-current/FLASH_MEMORY`,
  SHA-256 `78e78ae361ea5c7ff4a6a5ee11ebaa8c5042fee20dda2674ec01b20a29c1ac21`;
  `artifacts/logs/pc-vpcd-install-journal-current.log`, SHA-256
  `0240f6b29ed56671e885ff4ec174a001da8ca66f72ad3b1e73d007cf4d6dfa13`.

## 2026-10-21 — Verify CAP install journal across emulator process restart

- Step/experiment: `cap-install-process-restart-recovery`
- Change: Added a VPCD regression mode that injects a skipped CAP registry
  publication, leaves the committed install journal pending, shuts down the
  first emulator, and verifies recovery/SELECT in a fresh process using the
  saved flash image. The runner records the actual dynamic package ID chosen
  during LOAD. Emulator shutdown now waits up to five seconds after socket
  close for normal save/exit before sending a signal. Host CAP package lookup
  marks a supplied CAP image present only in resume mode, while initial LOAD
  still registers the package through its normal path.
- Validation: `python3 -m py_compile tools/tests/pc_vpcd_applet_test.py`,
  `./tools/build arm`, `./tools/build-pc`, and
  `./tools/test-persistent-transaction` passed. Elevated host-only VPCD restart
  test passed: package 57 recovered in the new process, SELECT returned
  `9000`, JCAlgTest 1.8.2 GETVERSION and JCSystem calls passed, SHA-256 support,
  doFinal, and static performance APDUs passed, and summary was
  `TEST:SUMMARY pass=11 fail=0`. Existing same-process recovery injection also
  passed `pass=11 fail=0`. RISC-V and physical USB/flash validation were
  intentionally skipped.
- Result: The durable journal and registry replay survive emulator process
  restart at the registry-publication gap. Testing also exposed a VPCD test
  harness race: immediate SIGINT after socket close could prevent the emulator
  from saving the image; natural transport shutdown fixes it. This does not
  cover all power-cut boundaries.
- Decision: Keep install atomicity open. Next inject after registry publish,
  during journal cleanup, and on CAP metadata write failure, then add cut-boundary
  coverage.
- Commit: Not created; `.git` metadata/index are read-only in this execution
  environment and shared source files contain unrelated pre-existing changes.
- Artifacts/logs: `artifacts/results/deploy/install-journal-process-restart-v5/FLASH_MEMORY`
  SHA-256 `adc17e34cc04adf7413c87ed0b309509ee32c2f51f47b348b230218d12143412`;
  `artifacts/logs/pc-vpcd-install-journal-process-restart-v5.log` SHA-256
  `89a488f2712202f9819d86a77d8394d1b207f45249934eb2d9c35af51c7febc8`;
  same-process log `artifacts/logs/pc-vpcd-install-journal-inprocess-final.log`.

## 2026-10-21 — Run three physical deploy areas concurrently

- Step/experiment: `parallel-physical-core-crypto-service-20261021-223127`
- Change: Used the three visible Pico 2 readers to run one API 3.0.5 area per
  device concurrently. No firmware was flashed.
- Validation: Elevated host USB path:
  `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas
  Core,Crypto,Service --min-devices 3 --max-devices 3`. All worker processes
  exited 0 and all installed CAPs were removed. Crypto (D92E) completed 466
  calls; Core (E0E1) completed 291 calls (272 OK, 15 spec-allowed unsupported,
  four optional Ext calls not installed); Service (5CFC) completed 124 calls
  (94 OK, 28 existing wrong results, one SW_ERROR, one EXC_RUNTIME). Optional
  Ext INSTALL returned expected `6A88` and rollback passed. The coordinator
  marks that optional stage failed despite worker exit 0 and successful Core
  cleanup. No source change was validated by this physical run.
- Result: Three-reader parallel use works for this bounded one-area-per-reader
  batch; per-area historical outcomes remain unchanged.
- Decision: Retain one independent area per reader for predictable parallel
  sweeps; continue install recovery boundary testing offline.
- Commit: Not created; `.git` metadata/index are read-only here and the shared
  worktree contains unrelated changes.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-223127/`; CSV
  SHA-256 values: D92E `a0deba0720def10cb05f650d23bf7d39cb6981fcdc6ed4ce8b3935b042a524fa`,
  E0E1 `31182bb12643543bc2ec8f2246ed2e5f652fde9b8c7ab85113b55d053c540600`,
  5CFC `e6a59e244f9d94839e0af51ed331d74b43f5673f80966c828279e41115d21c06`.

## 2026-10-21 — Verify journal replay after registry publication

- Step/experiment: `cap-install-published-registry-restart`
- Change: Added `--verify-install-after-publication-restart` to the host VPCD
  regression runner. The first process now leaves the journal pending after
  successfully publishing the CAP applet entry; the second process must
  recognize the matching entry, clear the journal idempotently, and restore
  the applet.
- Validation: Python syntax compilation passed. Elevated host VPCD test passed:
  fresh process logged `VPCD:INSTALL_JOURNAL_RECOVERED package=57`, target
  applet SELECT returned `9000`, and the JCAlgTest APDU regression finished
  `TEST:SUMMARY pass=11 fail=0`. The test exercises the same save-before-exit
  path already validated in the missing-registry restart case. No firmware or
  physical USB test was needed for this host-only change.
- Result: Both recovery states across process restart are now covered: the CAP
  registry entry missing, and the matching entry already published. The actual
  metadata-write error/power-cut cases remain untested.
- Decision: Add actual CAP metadata write-error and cut-boundary injection next;
  keep the atomicity item open until then.
- Commit: Not created; `.git` metadata/index are read-only in this environment,
  and shared source files include unrelated pre-existing work.
- Artifacts/logs: `artifacts/results/deploy/install-journal-published-restart/FLASH_MEMORY`
  SHA-256 `adc17e34cc04adf7413c87ed0b309509ee32c2f51f47b348b230218d12143412`;
  `artifacts/logs/pc-vpcd-install-journal-published-restart.log` SHA-256
  `8de41d061f77a5fb94917894d4ebc7504688f60bfb4d097c95a5e2e782771ca1`.

## 2026-10-21 — Recover install journal after CAP metadata write error

- Step/experiment: `cap-install-metadata-write-error-recovery`
- Change: Added a host-only failure hook that makes CAP instance publication
  return an error once. Added `--verify-install-metadata-error-restart` to run
  initial installation without VPCD, persist the image with the committed
  journal, and then launch a fresh VPCD process to exercise boot replay.
- Validation: `python3 -m py_compile tools/tests/pc_vpcd_applet_test.py` and
  `./tools/build-pc` passed. Elevated host-only recovery run logged
  `VPCD:TEST_FAILED_CAP_INSTANCE_STORE package=57` and
  `ERROR: PC applet INSTALL failed stage=8 sw=6581`; the next process logged
  `VPCD:INSTALL_JOURNAL_RECOVERED package=57`, selected the applet (`9000`),
  and passed all 11 JCAlgTest APDU checks (`TEST:SUMMARY pass=11 fail=0`). The
  outer emulator exits 0 despite the internal INSTALL error; the regression
  asserts the explicit marker and status word. No physical flash was run.
- Result: Journal replay repairs an actual reported CAP instance write failure
  after the persistent filesystem transaction committed. Combined with the
  other restart cases, the missing-entry and already-published-entry recovery
  paths both pass.
- Decision: Next inject deterministic reset/power cuts at transaction,
  metadata publication, and cleanup boundaries; keep cross-store atomicity
  marked incomplete until then.
- Commit: Not created; `.git` metadata/index are read-only in this environment,
  and shared source files include unrelated pre-existing work.
- Artifacts/logs: `artifacts/results/deploy/install-journal-metadata-error-restart-retry/FLASH_MEMORY`
  SHA-256 `adc17e34cc04adf7413c87ed0b309509ee32c2f51f47b348b230218d12143412`;
  `artifacts/logs/pc-vpcd-install-journal-metadata-error-restart-retry.log`
  SHA-256 `4e4713749f5ab26ca832e095f87b9574baa98511ec3a27fb3f107ea1c2dfae3d`.

## 2026-10-21 — Exercise remaining deploy applets across three readers

- Step/experiment: `parallel-physical-deploy-remaining-areas-20261021`
- Change: Ran the deploy applets not covered by the preceding Core/Crypto/
  Service batch, distributing Security, Javacardx, JavacardxSec, JavacardxExt,
  JavacardxInt, and Biometry over the three visible physical Pico readers.
  No firmware was flashed.
- Validation: Security on D92 completed 901 calls (820 `UNSUPPORTED_PER_SPEC`,
  63 OK, 13 EXC_RUNTIME, 2 SKIPPED_BY_USER, 2 WRONG_RESULT, 1 SW_ERROR) and
  CAP cleanup passed. Javacardx, JavacardxSec, JavacardxInt, and Biometry
  returned expected missing-import `6A88`; all partial uploads rolled back.
  JavacardxExt returned `6F00` twice on D92 and `6A88` on other readers; all
  partial uploads rolled back. The repeated D92 failure is consistent with
  stale dynamic package bitmap state on old firmware but was not proven by
  direct package-registry inspection. All worker processes exited 0; the
  coordinator reports each refused optional applet as a failed stage.
- Result: Every deploy API area has now been exercised across the three boards
  using their already-installed firmware. Optional packages remain absent;
  this run does not validate current-source changes.
- Decision: Compare Security's non-OK rows against current-source VPCD before
  changing dispatch or crypto code. Do not treat missing imports or expected
  `NO_SUCH_ALGORITHM` as implementation defects. Track D92 `6F00` against the
  known stale-bitmap source fix; physical confirmation requires a permitted
  firmware flash after the architecture gate is resolved.
- Commit: Not created; `.git` metadata/index are read-only in this environment
  and the shared worktree contains unrelated pre-existing modifications.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-223749/`,
  `parallel-20261021-224015/`, and `parallel-20261021-224114/`. CSV SHA-256:
  Security D92 `53baaaf7fcbf8bd91f41abc92e765112da1cd7fb39a2c989032c309d1252829d`,
  Javacardx E0E `6be34c1497565317025fb258ce90db02a3ec29603d6bfb31fef792f322cef5e9`,
  JavacardxSec 5CFC `6530ed9d8ed03258d9bfa5b174225127797046cefc699506bd0eafc36dfe8dc4`,
  JavacardxExt D92 `1f39b4d8fbcae3e423e3a36e507f06d740264800f9706c809d1afc8c846dba88`,
  JavacardxInt E0E `6340424a65c76b0c0630ebcc99dc396b92cd86f0bbb29ac143a4e68aa2d6116f`,
  Biometry 5CFC `b4e7314c44db47ea09e27c736df69a356d36c2024e11a8f5cca3104bafa50efb`,
  JavacardxExt repeat `dc39fb0befe019ad2c2d5222eaf1a172b4c0ac70aea880a5f8c8c956395b2b9c`.

## 2026-10-21 — Correct VPCD applet regression check counts

- Step/experiment: `vpcd-applet-test-summary-count`
- Change: Replaced the fixed `pass=11` summary in
  `tools/tests/pc_vpcd_applet_test.py` with mode-based counts: full suite 15,
  select-only 2, custom APDU path `2 + N`, and two pre-restart checks when the
  process-restart fault test runs.
- Validation: Python syntax compilation passed. Full JCAlgTest VPCD regression
  reports `TEST:SUMMARY pass=15 fail=0`; Security CAP install/select reports
  `pass=2`; skipped-registry process restart recovers package 57 and reports
  `pass=17`. Individual SHA-256, JCSystem, SELECT, and APDU checks passed.
- Result: Current summaries now match executed checks. Older logs still print
  `pass=11`; their individual PASS lines remain authoritative, but the numeric
  summary undercounts and must not be reused as an exact check count.
- Decision: Apply the corrected counting rule to future VPCD results; use the
  current 15/2/17 summaries for regression reporting.
- Commit: Not created; `.git` metadata/index are read-only in this environment
  and the worktree includes extensive unrelated changes.
- Artifacts/logs: Full JCAlgTest
  `artifacts/logs/jcalgtest-vpcd-accurate-summary.log` SHA-256
  `b97a06dc55d132e0fb28b9ba4f551c0bba1e7ec8c0aacd11e3f0d96a9f3b3d77`;
  Security select-only
  `artifacts/logs/deploy-security-select-vpcd-current-accurate.log` SHA-256
  `3c3807f5ab6a14c4618a763a1f6f5a6903843e87554f52abfae378ae9b142312`;
  restart `artifacts/logs/pc-vpcd-install-journal-process-restart-accurate.log`
  SHA-256 `097f47849e47c526cd1c0c27633da63699ac24f3038dbf4c67a35fc98a12e02d`.

## 2026-10-21 — Pin reader indexes for parallel physical deploy tests

- Step/experiment: `parallel-deploy-reader-index-pinning-20261021`
- Change: The parallel coordinator now captures each Pico's Java SmartcardIO
  index with its exact PC/SC name and passes the index plus a GPPro `-r`
  selector to the worker. This skips a second concurrent reader enumeration
  while still pinning GPPro to the exact reader name.
- Validation: Python compilation and `git diff --check` passed. First full
  physical sweep found three readers but the third worker's concurrent Java
  lookup exited before creating a CSV; the same exact lookup succeeded alone
  at index 5. With the final index-plus-GPPro-selector invocation, the 3-reader
  Security/Crypto/Service run completed all three workers and all uploads,
  tests, and cleanup stages. No firmware was flashed.
- Result: Security: 901 calls (820 `UNSUPPORTED_PER_SPEC`, 63 OK, 13
  `EXC_RUNTIME`, 2 `SKIPPED_BY_USER`, 2 `WRONG_RESULT`, 1 `SW_ERROR`). Crypto:
  466 calls (434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, 11 OK, 3
  `EXC_CARDRUNTIME`, 2 `SW_ERROR`). Service: 124 calls (94 OK, 28
  `WRONG_RESULT`, 1 `SW_ERROR`, 1 `EXC_RUNTIME`). `JavacardxSec` in the first
  unconstrained full sweep returned INSTALL `6A88`. Its CAP Import component
  lists `A000000062020901` and `A0000000620205`; this is not a dependency on
  the separate Security test CAP. Whether the board should provide these JC
  API packages needs review.
- Decision: Keep the index snapshot and exact GPPro name paired per worker.
  Treat `JavacardxSec` 6A88 as a missing imported API package pending profile
  review, not a dependency-scheduling failure. Investigate the recorded
  Security/Crypto/Service non-OK rows separately; expected unsupported calls
  are not defects.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: First sweep `artifacts/results/deploy/parallel-20261021-225120/`;
  final pinned confirmation `artifacts/results/deploy/parallel-20261021-225932/`.
  Confirmation CSV SHA-256: Security
  `4df6eda84be6f68f8df95cc26a766c0460a5bf7714f67c478a2bdc87782714e2`, Crypto
  `b7eaaad0a513f9f23f6c5eb3d9bd170da598e64366f271213c0d2061b08324bb`, Service
  `3f4993cdbddfb15f1bb98b0111c12fa9a419d5adf075a75cac48f095d61c8a44`.


## 2026-10-21 — Confirm three-reader setup and rerun parallel deploy APIs

- Step/experiment: `three-reader-physical-recheck-20261021`
- Change: Rechecked PC/SC visibility after the user ran the updated installer
  helper, then exercised one API 3.0.5 area on each Pico concurrently. No
  source or firmware was changed.
- Validation: Elevated `pcsc_scan -r` listed D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437. The elevated parallel runner pinned
  Java reader indices 3, 4, and 5 to those exact names. Crypto completed 466
  calls and cleanup; Service completed 124 calls and cleanup; Core completed
  291 calls and cleanup. Core's optional Ext INSTALL returned `6A88`; the
  partial upload rollback passed. Coordinator exit was 1 solely because that
  optional Ext stage is recorded as refused. Total run time was about 50 sec.
- Result: All three PC/SC readers are usable concurrently with exact routing.
  Crypto outcomes: 434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, 11 OK, 3
  `EXC_CARDRUNTIME`, 2 `SW_ERROR`. Core: 272 OK, 15
  `UNSUPPORTED_PER_SPEC`, 4 optional Ext calls unavailable. Service: 94 OK,
  28 `WRONG_RESULT`, 1 `SW_ERROR`, 1 `EXC_RUNTIME`. No firmware was flashed;
  results describe the images currently on the devices.
- Decision: Keep assigning one independent deploy area per reader. Preserve
  expected unsupported calls and optional `6A88` separately from runtime bugs.
- Commit: Not created; `.git` is read-only in this environment and the shared
  worktree has extensive unrelated modifications.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-231607/`. CSV
  SHA-256: D92 Crypto
  `202ea769fb83a6fa0b01fb4f7454423119ab560ada5f20b6fde9c19345deb5a8`, E0E Core
  `807e69453bee86d9842195e300a273e55fb3f921f3b99e545f8dd6a550569ed5`, 5CFC
  Service `817f061f77f2f0d17744c4a0156e98ce68f099902b986fbc395375b7a4482122`.


## 2026-10-21 — Reject partial API results after client failure

- Step/experiment: `api-runner-client-exit-status`
- Change: Updated `AlgTest_JavaCard/deploy/api_card_runner.py` to fail the
  TEST stage with `TEST_CLIENT_ERROR` when the Java client exits nonzero, even
  if it produced a partial CSV. Partial rows remain available for diagnosis.
- Validation: Synthetic client wrote one row then returned status 143; runner
  retained the row and emitted a failed TEST stage with `TEST_CLIENT_ERROR`.
  `python3 -m py_compile AlgTest_JavaCard/deploy/api_card_runner.py` and
  `git diff --check` passed. No hardware/VPCD access was used for this check.
- Result: Interrupted clients can no longer appear as completed API test runs.
  The earlier 551-row host Security CSV is partial and not a passing result.
- Decision: Resume the exact Security APDU stall investigation with the
  corrected runner and treat abnormal child exit as test failure.
- Commit: Not created; `.git` is read-only in this environment.
- Artifacts/logs: Code change only; synthetic validation was ephemeral.
## 2026-10-21 — Run remaining deploy areas across three physical readers

- Step/experiment: `three-reader-extension-deploy-20261021`
- Change: Used the three-reader coordinator for Security, Javacardx,
  JavacardxExt, JavacardxSec, JavacardxInt, and Biometry API 3.0.5 areas.
  No firmware was flashed.
- Validation: All three exact reader names were enumerated. Security on
  D92E743D5F640C0F completed 901 calls and cleanup: 820
  `UNSUPPORTED_PER_SPEC`, 63 `OK`, 13 `EXC_RUNTIME`, 2
  `SKIPPED_BY_USER`, 2 `WRONG_RESULT`, 1 `SW_ERROR`. Javacardx,
  JavacardxSec, Biometry, JavacardxExt, and JavacardxInt INSTALL were refused
  with `6A88`; each partial LOAD was removed. Read-only GPPro `--list` checks
  against the exact three reader names confirmed all were physical CHOUPI
  readers; the first contained JCAlgTest and the other two had empty applet
  registries.
- Result: Parallel routing and cleanup worked. The five `6A88` outcomes are
  missing imported package dependencies; they do not demonstrate runtime
  failures in those API areas.
- Decision: Keep these API package-profile dependencies separate from VM
  correctness. Continue the CAP install atomicity/power-cut coverage.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-232828/`;
  CSV SHA-256: D92 Security
  `eaf16b72e299ee9aa54b34261b33a8094ea508b3ea2c59b8b058326bf7ad66de`,
  E0E Javacardx/JavacardxSec/Biometry
  `cceb553c9f6c5d5e578a7ec661ccd76cb704d3cdd8be1530b0aa42900ffb4457`,
  5CFC JavacardxExt/JavacardxInt
  `1194ee70e972417964a6e0e8a238267d7c0c5b28c92b93934f9ae2fe85e9aa23`.


## 2026-10-21 — Fix reader-name test routing and verify package coexistence

- Step/experiment: `physical-multi-package-coexistence-and-reader-name`
- Change: `api_card_runner.py --reader-name` added the GPPro selector but did
  not resolve the matching SmartcardIO index, so the official Java test waited
  at its reader prompt and exited 1. It now resolves the exact name before
  launching the client.
- Validation: `python3 -m py_compile AlgTest_JavaCard/deploy/api_card_runner.py`
  and targeted `git diff --check` passed. Physical Core and Crypto CAPs were
  installed on E0E14525E64AF6ED with Core retained while Crypto loaded. GPPro
  listed both packages and applets as LOADED/SELECTABLE. With both resident,
  the official client completed Core (291 calls: 272 OK, 11
  `UNSUPPORTED_PER_SPEC`, 8 scenario methods unavailable because Multi/Ext
  scenarios were disabled) and Crypto (466 calls: 434
  `UNSUPPORTED_PER_SPEC`, 11 OK, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 2
  SW_ERROR). Crypto and Core were then uninstalled; final GPPro `--list` showed
  only the ISD.
- Result: Multi-package retention, independent applet selection, exact-reader
  name routing, and cleanup all worked on the firmware already installed on
  this Pico. The current source image was not flashed; these results do not
  validate source changes on hardware.
- Decision: Keep the reader-name fix. Next continue the open CAP install
  durability and GlobalPlatform lifecycle items; analyze only concrete VM
  execution failures, not expected unsupported algorithms.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `AlgTest_JavaCard/deploy/results/multi-package-physical-20261021/`;
  CSV SHA-256: Core upload/initial client failure
  `72535b638a43d228395b4a10663f54200ca59a6184866f238d06ec1b89673b96`, Core
  retest `3cc332b043a6ca5e231ed087bb08b213c2b78bfe4aba52636bdfa2edec8f5019`,
  Crypto `4b5197f2d86cd140656b4d2a7f5349465aeabf8480e92cb4edc7f703a26740ce`,
  coexistence retest `e6a46014e974fbd0d26317b41815e6297b4b5c4cb4a5291ec84018a1174fd437`.

## 2026-10-21 — Resolve canonical JC305 JavacardxSec imports

- Step/experiment: `jc305-javacardxsec-import-aid-aliases`
- Change: `Import_Handler::getPackageIndex()` now accepts two exact aliases
  from standard JC305 import AIDs to the corresponding CHOUPI ROM package AIDs:
  `javacardx/apdu/util` and `javacardx/security`. Imported static method
  tokens for APDUUtil and SensitiveResult are shifted by one to account for
  the constructor token in the local ROM exports.
- Validation: `./tools/build-pc` and `./tools/build arm` passed; ARM image end
  `0x100c6c00`. `./tools/test-install-status` passed 11/11. The official
  JavacardxSec deploy CAP installed and selected through host VPCD
  (`TEST:SUMMARY pass=2 fail=0`). The standard JCAlgTest VPCD regression passed
  15/15, including SHA-256 `doFinal`, static 50-call workload, and expected
  `NO_SUCH_ALGORITHM`. No RISC-V build or physical flash was performed.
- Result: The `6A88` import lookup cause is confirmed in current source and the
  two canonical imports now link to the existing ROM packages. This is not
  full JavacardxSec API support: generated `SensitiveResult` methods are
  currently no-op stubs; the 84-call API behavior has not been run on current
  source.
- Decision: Keep the exact aliases and token maps. Next investigate and test
  SensitiveResult semantics; leave this API area open until its calls have
  meaningful behavior or are explicitly rejected by the profile.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/deploy-javacardxsec-import-alias-vpcd-20261021.log`
  SHA-256 `f2d0b70345eb66ae675eb285d2c0ed2b74652e7f884775a879a1625cef4c6c8b`;
  `artifacts/logs/jcalgtest-post-import-alias-vpcd-20261021.log` SHA-256
  `9b8a287a2c88a09dda2e2977be58bb888439f2c3824fd1dafbc8ac99b8133a0b`;
  `build/arm/pico2_choupi.uf2`.

## 2026-10-21 — Run deploy API coverage across three attached Picos

- Step/experiment: `three-reader-deploy-api-current-firmware`
- Change: Confirmed the three-reader PC/SC setup after the approved reader helper
  ran, then used the parallel runner with exact Java reader indices 3–5. Ran
  Core, Crypto, and Service in one batch and the five remaining deploy areas in
  a second batch. No firmware was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --areas Core,Crypto,Service --min-devices 3 --max-devices 3 --dry-run`
  found D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. The live
  three-worker batch completed in about 45 seconds. Crypto: 466 calls, 11 OK,
  434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, 3 `EXC_CARDRUNTIME`, 2
  `SW_ERROR`; cleanup passed. Core: 291 calls, 271 OK, 15
  `UNSUPPORTED_PER_SPEC`, four optional Ext calls unavailable, one
  `Applet.deselect()` `WRONG_RESULT`; Multi applet creation and cleanup passed.
  Optional Ext INSTALL returned `6A88`, and rollback passed. Service: 124
  calls, 94 OK, 28 `WRONG_RESULT`, one `SW_ERROR`, one `EXC_RUNTIME`; cleanup
  passed. The five remaining areas were then distributed across the same
  readers. Javacardx and JavacardxInt INSTALL returned `6F00`; JavacardxExt,
  JavacardxSec, and Biometry returned `6A88`. All five partial LOAD rollbacks
  passed, and no API calls ran for those refused areas.
- Result: Three-reader routing, concurrent deploy execution, and rollback work
  in this session. The client logs still print `Virtual PCD` in GPPro error
  text, but the coordinator enumerated and pinned the three distinct physical
  Pico names. Results apply to the already installed firmware, not the current
  source. The `6F00`/`6A88` install refusals are package/install issues, not
  evidence of VM method execution failures. Core deselect and Service API
  behavior remain concrete runtime mismatches for diagnosis.
- Decision: Keep one independent API area per Pico worker for repeatable
  parallel runs. Investigate Core `Applet.deselect()` and Service behavior
  separately; continue import/profile analysis for refused packages. Do not
  attribute expected unsupported algorithm outcomes to VM defects.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261021-234459/` and
  `artifacts/results/deploy/parallel-20261021-234630/`. First-batch CSV SHA-256:
  Crypto `72f7ef3128ee2f1563736b3f35697d8db7d8705dcf7f0b67b8a7d289909f1eaf`,
  Core `dd7a90d8a9b75882036a03f35100fa89119a3e9547bfaa53aa4254f0849a6291`,
  Service `11e7ff041cfdd59b2559ccbd0694c28ce9ca7fb50af4412739623aa7a2701808`.
  Second-batch CSV SHA-256: Javacardx/JavacardxInt
  `0e360a27603de1ad6c48c1faab02490e5802bc6be3c710035ee2dde68ab54851`,
  JavacardxExt/Biometry
  `21b5df3f7b3a9c582f56de645c1ded6ac40968bd89621bb82d3b28da791a4eea`,
  JavacardxSec
  `b16263d8700e1b63c2b17646c20f525c816acb58696118b87e7ac72093f5ace7`.

## 2026-10-22 — Validate three-reader routing and JC305 Dispatcher semantics

- Step/experiment: `three-reader-service-dispatcher-current-source`
- Change: Reused the three-reader PC/SC setup and exact-reader parallel coordinator. Validated current-source `Dispatcher` API results using the official JCAlgTest deploy catalog's decoder; no source edits or firmware flash were made in this step.
- Validation: `pcsc_scan -r` before and after the physical run listed all three readers (D92E743D5F640C0F, E0E14525E64AF6ED, 5CFC5328F5BDE437). The parallel Core/Crypto/Service command ran all three workers and cleaned each installed CAP. Crypto completed 466 calls (11 OK, 434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, three `EXC_CARDRUNTIME`, two `SW_ERROR`). Core completed 291 calls (272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext calls not run); the optional Ext CAP INSTALL returned `6A88` and rollback passed. Service completed 124 calls (94 OK, 28 `WRONG_RESULT`, one `SW_ERROR`, one `EXC_RUNTIME`). The coordinator exited 1 because it counts that optional Ext INSTALL refusal as a failed stage; all worker processes exited 0 and cleanup passed. Separately, ARM and host builds recorded in the previous Service Dispatcher step passed. Current-source VPCD installed and selected the Service CAP, then ran all 33 Dispatcher cases; official decoder result: `SERVICE_API:SUMMARY total=33 pass=33 fail=0`, and harness result: `TEST:SUMMARY pass=35 fail=0` (ATR, SELECT, and 33 APDUs).
- Result: Exact-reader routing and three-reader concurrency remain usable. Current-source Dispatcher constructor/add/remove/dispatch/process results match all 33 official expected results. Physical Service API mismatches are from firmware already installed on the boards and do not contradict the current-source VPCD result. RMIService remote invocation remains a stub.
- Decision: Preserve the optional Ext refusal as a dependency/profile issue; next validate current-source BasicService API cases and then scope the remaining RMIService behavior. Do not flash while ARM-only scope conflicts with the repository's both-architecture flash gate.
- Commit: Not created; repository metadata is read-only in this environment.
- Artifacts/logs: physical results in `artifacts/results/deploy/parallel-20261022-current/`; current-source VPCD output in `artifacts/logs/service-dispatcher-official-decode-vpcd.txt` and emulator trace in `artifacts/logs/service-dispatcher-official-decode-vpcd-emulator.log`.

## 2026-10-22 — Validate BasicService API cases against the official decoder

- Step/experiment: `jc305-basic-service-vpcd-decoder`
- Change: Extended a temporary build-only decoder to run all BasicService cases and the RMIService catalog cases against current-source VPCD. No repository source was changed.
- Validation: The Service CAP installed and selected; 39 BasicService cases plus five RMIService catalog cases returned data matching the official `ApiTesting` decoder: `SERVICE_API:SUMMARY total=44 pass=44 fail=0`; transport harness `TEST:SUMMARY pass=46 fail=0` (ATR, SELECT, 44 APDUs). `BasicService` input/output state checks, common output format, status words, success/fail helpers, and CLA/INS/P1/P2 matched. RMIService constructor null handling matched. API catalog checks do not exercise a complete Java Card RMI SELECT/INVOKE exchange; `RMIService.processCommand()` remains a stub, and `setInvokeInstructionByte()` has no retained state. No physical card was modified in this step.
- Result: Current-source VPCD passes the tested BasicService and direct RMIService API cases. Full RMI protocol support is still missing, so the whole Service framework is not complete.
- Decision: Next inspect the SDK's RMI wire-format and object-export helpers before implementing or claiming remote invocation support. Continue to avoid flashing under the ARM-only constraint and both-architecture flash gate.
- Commit: Not created; repository metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/service-basic-official-vpcd.txt`, `artifacts/logs/service-basic-official-decoded.txt`, `artifacts/logs/service-rmi-official-vpcd.txt`, and `artifacts/logs/service-rmi-official-decoded.txt`.

## 2026-10-22 — Exercise three physical readers concurrently

- Step/experiment: `three-reader-core-crypto-service-rerun-20261022`
- Change: Ran one JC305 deploy area per attached Pico through the exact-reader parallel runner; no firmware was flashed.
- Validation: Elevated `pcsc_scan -r` listed physical readers D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas Core,Crypto,Service --min-devices 3 --max-devices 3` ran all workers concurrently. Crypto completed 466 calls (11 OK, 434 `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, three `EXC_CARDRUNTIME`, two `SW_ERROR`) and cleanup passed. Core completed 291 calls (272 OK, 15 `UNSUPPORTED_PER_SPEC`, four optional Ext calls unavailable); optional Ext INSTALL returned `6A88`, then rollback and Core cleanup passed. Service completed 124 calls (94 OK, 28 `WRONG_RESULT`, one `SW_ERROR`, one `EXC_RUNTIME`) and cleanup passed. Worker exit codes were all 0; coordinator exit was 1 solely because it counts optional Ext INSTALL refusal as a failed stage.
- Result: Three-reader routing and concurrent test execution work. Core/Crypto/Service results describe installed board firmware, not the current source. No sequential baseline was measured, so speedup is not quantified.
- Decision: Keep exact reader pinning and per-worker artifact logs. Continue current-source VPCD testing for code changes; do not flash while the ARM-only scope conflicts with the repository flash gate.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-004140/`; CSV SHA-256: Crypto `04d59e1af3ffd58e3618769904141e3cb097a9b4fa1a028bb71d397d96e5b9e3`, Core `17f6f7c7512160d06b29d325687f33c0978123f30e546794fd4cbca87ca6bbd8`, Service `6b36c15731faeecdc4ec9b659ba4c5e7f10390238257bc0ec3cd14412f4778ea`.

## 2026-10-22 — Correct JC305 MessageDigest constructor dispatch

- Step/experiment: `jc305-message-digest-constructor-token-map`
- Change: Added a scoped imported-constructor token translation for `javacard.security.MessageDigest`: JC305 token 1 maps to embedded ROM export token 2 when resolving `invokespecial`. The ROM table probe showed tokens 0 and 1 have two arguments, while token 2 has the receiver-only signature. This prevents entering a factory method as the superclass constructor.
- Validation: Before the fix, the official Security call `MessageDigest()` from a subclass returned `6F00` with `StackUnderflowException`. After the fix, the isolated call returned `049000` and `TEST:SUMMARY pass=3 fail=0`. The full 901-case Security CAP completed with `TEST:SUMMARY pass=903 fail=0`. Official result decoding: 64 `OK`, 822 spec-allowed `UNSUPPORTED_PER_SPEC`, 13 `EXC_RUNTIME`, and two `WRONG_RESULT`; expected unsupported algorithms remain allowed. `./tools/build arm` and `./tools/build-pc` passed. Host build emitted existing Rust and packed-member warnings. RISC-V build and physical flash were intentionally skipped under the ARM-only scope and repository flash gate.
- Result: The MessageDigest subclass constructor path now executes through the full Security matrix. Remaining runtime statuses and value mismatches are recorded for later triage; this step does not claim complete Security API support.
- Decision: Preserve this narrow constructor mapping. Next resume the higher-level Service RMI protocol gap, keeping unsupported algorithms distinct from defects.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/security-case407-final.txt`, `artifacts/logs/security-case407-final-emulator.log`, `artifacts/logs/security-full-vpcd-fixed.txt`, `artifacts/logs/security-full-vpcd-fixed-decoded.txt`, `artifacts/logs/security-full-vpcd-fixed-emulator.log`, `artifacts/logs/build-arm-security-constructor-fix.log`, and `artifacts/logs/build-pc-security-constructor-fix-final-normal.log`. Full harness output SHA-256: `1a8aa0c20cbd35b56f8d145b6103980118889baedf9016a1ecc77bc7bf75de14`; decoded output SHA-256: `a73b94b268a0ff5e5153164a67366ee4eab44b4abd4be2ec9da3a73c6343359b`.


## 2026-10-22 — Run current firmware deploy APIs across three readers

- Step/experiment: `three-reader-core-crypto-service-rerun-20261022-012453`
- Change: Refreshed USB permissions with the approved helper, confirmed all three physical CHOUPI PC/SC readers, and ran one JC305 deploy area on each reader concurrently. No device was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader`, `PICO2_HOST_USB_ACCESS=1 pcsc_scan -r`, and `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas Core,Crypto,Service --min-devices 3 --max-devices 3` (host access via elevated execution). Readers: D92E743D5F640C0F, E0E14525E64AF6ED, 5CFC5328F5BDE437. Crypto completed 466 calls (11 OK, 434 spec-allowed unsupported, 16 EXC_RUNTIME, three EXC_CARDRUNTIME, two SW_ERROR) and cleanup passed. Core completed 291 calls (272 OK, 15 spec-allowed unsupported, four optional Ext calls unavailable); optional Ext INSTALL returned 6A88, rollback passed, and Core/Multi cleanup passed. Service completed 124 calls (94 OK, 28 WRONG_RESULT, one SW_ERROR, one EXC_RUNTIME) and cleanup passed. All workers exited 0; coordinator exit 1 because optional Ext refusal is counted as a failed stage.
- Result: Three-way reader routing and concurrent deploy execution remain functional. The measured API results describe the firmware already on the boards, not current source; no speedup baseline was measured.
- Decision: Keep exact-reader assignment and one area per worker. Diagnose current-source Service behavior through VPCD; do not flash under ARM-only scope and the repository's two-architecture flash gate.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-012453/`; CSV SHA-256: Crypto `52799dfb81efbe56168b87a2e726118ef6cfc8dd0e5a1f06b12af0a73f4a15da`, Core `ca6379cf6ed1fe48a8627fdad7c9197878492402958dde42cbb3f66408637613`, Service `41a56291602347c7b534c78db30d04f1580f11f95ba6c4bf546d5443378a114c`.


## 2026-10-22 — Validate the current-source Service deploy matrix

- Step/experiment: `jc305-service-api-vpcd-current-rmi-state`
- Change: Re-ran all 124 JC305 Service API probes against current-source host VPCD using the Service CAP's actual applet AID, `4A43416C675465737441706953766331` (`JCAlgTestApiSvc1`). No VM source was changed in this validation step.
- Validation: The initial attempt used the Security AID (`...53656331`) and failed CAP INSTALL at stage 4/`6A82` before APDUs. Reading the CAP's `Applet.cap` showed the correct Service AID; the corrected run installed and selected the CAP and completed with `TEST:SUMMARY pass=126 fail=0` (ATR, SELECT, 124 APDUs). Official decoder: `SERVICE_API:SUMMARY total=124 pass=124 fail=0`. ARM and host builds from the immediately preceding implementation step passed. No RISC-V build, physical flash, or current-source physical test was performed.
- Result: The earlier stage-4 failure was a test AID mismatch, not a loader regression. Service catalog behavior including the RMIService public API entry points matches expected return values, but catalog tests do not exercise an RMI SELECT/INVOKE exchange or exported-object access.
- Decision: Keep the Service applet AID sourced from each CAP rather than reusing an AID from another API area. Resume RMI wire protocol design and implementation; no flash under ARM-only scope and the two-architecture flash gate.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/service-rmi-state-vpcd-correct-aid.txt` (SHA-256 `2e34ba668f2b0370a199ca0bc134e4b7f5da6d1f95277f50d472558eb1cff0da`), `artifacts/logs/service-rmi-state-vpcd-correct-aid-emulator.log`, and `artifacts/logs/service-rmi-state-vpcd-correct-aid-decoded.txt`.


## 2026-10-22 — Retain RMIService initial reference and invoke INS state

- Step/experiment: `rmi-service-selection-state-foundation`
- Change: `RMIService` now retains its initial `Remote`, tracks the active and pending invoke instruction bytes, stages updates in `setInvokeInstructionByte()`, and provides a selection-session activation helper. This is groundwork only; the selection path does not call the helper yet.
- Validation: `./tools/build arm` passed (`artifacts/logs/build-arm-rmi-session-state.log`); `./tools/build-pc` passed (`artifacts/logs/build-pc-rmi-session-state.log`). Correct-AID host VPCD Service CAP install and all 124 official API catalog cases passed (`SERVICE_API:SUMMARY total=124 pass=124 fail=0`). This catalog verifies public call completion, not that a subsequent SELECT returns the changed INS or exports an object reference. No RISC-V build, current-source physical flash, or device flash was performed.
- Result: The Java object has the state needed for deferred INS activation. Complete RMI SELECT response construction, object export tracking, and INVOKE execution remain unimplemented.
- Decision: Implement the selection protocol slice next and add a fixture that observes active INS and the initial remote reference end-to-end.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: ARM and host build logs above; Service VPCD artifacts are recorded in `jc305-service-api-vpcd-current-rmi-state`.


## 2026-10-22 — Add per-applet exported remote-object registry

- Step/experiment: `rmi-exported-object-registry`
- Change: `CardRemoteObject.export()` and `unexport()` now call VM natives backed by a per-applet 32-slot registry. Entries get stable non-null 16-bit IDs, are visited as GC roots, can be re-exported with their original ID, and unexported entries are no longer returned by lookup. Capacity is deliberately above the Java Card minimum to account for existing applet references.
- Validation: `./tools/build arm` and `./tools/build-pc` passed. Current-source Service CAP VPCD run completed 124 APDUs with `TEST:SUMMARY pass=126 fail=0`; official decoder reports `SERVICE_API:SUMMARY total=124 pass=124 fail=0`. The 16-object case failed at first with `SystemException.NO_RESOURCE` when the registry had 16 slots, because the applet already had remote objects registered; it passed after increasing capacity to 32. No RISC-V build, physical flash, or current-source physical validation was performed.
- Result: Export/unexport API behavior and the deploy CAP's 16-object capacity scenario pass. Direct GC survival assertions and RMI session-retention behavior are not covered; RMI SELECT/INVOKE remains unimplemented.
- Decision: Keep the 32-entry bounded registry. Next add explicit per-selection references and connect them to RMI SELECT; then implement reference encoding and INVOKE dispatch.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/build-arm-rmi-export-registry-final.log`, `artifacts/logs/build-pc-rmi-export-registry-final.log`, `artifacts/logs/service-rmi-export-registry-final.txt` (SHA-256 `805fe719b026f7f5462d63c749c43c55a2c011c1d63d7434dae413bf0785039c`), and `artifacts/logs/service-rmi-export-registry-final-decoded.txt`.


## 2026-10-22 — Track RMI references published in the active selection

- Step/experiment: `rmi-selection-reference-retention`
- Change: Added a session-reference bit to exported-object entries. Unexport now releases unpublished objects immediately; a reference explicitly retained for the current selection remains resolvable until applet deselection, when unexported entries are removed. `NativeImplementation.retainRemoteObjectForSession()` exposes the operation, and `RMIService.beginSelectionSession()` now activates its pending invoke INS and retains its initial object. The method is not yet called from `processCommand()` because SELECT response encoding is not implemented.
- Validation: New `./tools/test-rmi-registry` passes (`RMI_REGISTRY:PASS ids=32 roots=1 session_cleanup=1`), covering stable IDs, capacity, exported-object GC-root enumeration, session retention through unexport, and cleanup on deselection. `./tools/build arm` and `./tools/build-pc` pass. Current-source Service VPCD completed all 124 cases (`TEST:SUMMARY pass=126 fail=0`; `SERVICE_API:SUMMARY total=124 pass=124 fail=0`). No RISC-V build, physical flash, or current-source physical test was run.
- Result: Registry session behavior has focused host evidence. It is not yet wired to an actual RMI SELECT, and full object-reference encoding/remote invocation remain open.
- Decision: Next integrate the session helper into the RMI SELECT path and add a VPCD fixture for the initial-reference response and configured INS.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `tools/tests/rmi_registry/`, `tools/test-rmi-registry`, `artifacts/logs/build-arm-rmi-session-retention.log`, `artifacts/logs/build-pc-rmi-session-retention.log`, `artifacts/logs/service-rmi-session-retention.txt` (SHA-256 `3aa76be744648acb0550c2f5dc3d8d107951cfed8ac1a7bc6990080bc68d27cc`), and `artifacts/logs/service-rmi-session-retention-decoded.txt`.


## 2026-10-22 — Revalidate three-reader parallel deploy setup

- Step/experiment: `three-reader-core-crypto-service-rerun-20261022-014958`
- Change: After the PC/SC permission helper ran, confirmed the three physical readers and executed the existing three-worker JC305 deploy runner. No firmware was flashed and no source code changed.
- Validation: Elevated `pcsc_scan -r` listed physical readers D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437 at Java reader indices 3, 4, and 5 (virtual readers were indices 0–1). `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas Core,Crypto,Service --min-devices 3 --max-devices 3` assigned Crypto/Core/Service to separate readers. Crypto completed 466 calls (11 OK, 434 spec-allowed unsupported, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 2 SW_ERROR); Core completed 291 calls (272 OK, 15 spec-allowed unsupported, 4 optional Ext calls unavailable); Service completed 124 calls (94 OK, 28 WRONG_RESULT, 1 SW_ERROR, 1 EXC_RUNTIME). All workers exited 0 and all three CAP cleanup stages passed. Core's optional Ext INSTALL returned `6A88`, rollback passed, and this expected unavailable optional package caused coordinator exit 1.
- Result: The PC/SC helper and parallel runner can route concurrent deploy tests to all three live Pico readers. Results describe the firmware currently on the cards, not current source. No performance speedup baseline was measured.
- Decision: Keep exact reader-name routing and three-way independent-area assignment. Resume current-source RMI SELECT/INVOKE implementation and VPCD validation; no flash under the ARM-only scope and repository flash gate.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-014958/`; CSV SHA-256: Crypto `1ec235a111705a8b07fbf4bb071c9ec86ec0c405881560d74f9665ef76409a5a`, Core `8dbbfded2752d398cf61302c8f3d25fdeb9241d285fe2f61a6b475e9cf94175e`, Service `17fe5207062c6061a9e983f9a959773cfe55a12ec097178457ea5f4519ce9f2e`.


## 2026-10-22 — Identify missing metadata for RMI references

- Step/experiment: `jc305-rmi-reference-name-metadata-inspection`
- Change: Inspected the RMI API, JC305 CAP contents, and Runtime Environment Specification to establish what blocks a correct SELECT reference descriptor. No VM source changed.
- Validation: The official JC305 spec defines SELECT response FCI tags `6F/6E/5E`, version `0202`, invoke INS, and a remote reference descriptor; its default class format requires package/class names and the alternate format requires interface names. The deploy Service CAP contains Class/Method/Descriptor components and packaged `.class` resources, but no `Debug.cap`; runtime sources contain no Debug component parser. This is static inspection only; no RMI APDU was run.
- Result: Encoding an object ID alone would produce an invalid/incomplete interoperable RMI reference. Supporting standard optional Debug component metadata is a prerequisite for naming types, unless a separate standards-compliant metadata source is found.
- Decision: Add Debug component support/metadata retention to the RMI prerequisites before implementing SELECT FCI; retain the current ARM-only/no-flash scope.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Inspection of `AlgTest_JavaCard/deploy/caps/AlgTestApiService_305.cap` and `choupi/src/jc_cap/`; official reference: [JC305 Runtime Environment Specification, RMI sections](https://docs.oracle.com/cd/E59935_01/JCCRE.pdf).

## 2026-10-22 — Correct RMI metadata source and refresh three-reader access

- Step/experiment: `rmi-cap-metadata-correction-and-reader-refresh`
- Change: Corrected the earlier conclusion that RMI type names require the optional Debug component. The Java Card VM spec defines remote class metadata in Class component records for CAP 2.2+. Recorded that the supplied Service CAP is format 2.1 and cannot validate that metadata. Updated `TODO.md`, `.agent/QKB.md`, and `.agent/HANDOFF.md`; the existing Class remote-info parser is explicitly treated as an incomplete foundation pending a real CAP 2.2 fixture.
- Validation: Refreshed USB permissions with `sudo -n /usr/local/sbin/pico2-pcsc-reader` and ran elevated `pcsc_scan -r`; all three physical readers appeared: D92E743D5F640C0F, E0E14525E64AF6ED, 5CFC5328F5BDE437. `./tools/test-rmi-cap-metadata` passed (`RMI_CAP_METADATA:PASS methods=1 class_name=Impl interfaces=1`); `./tools/test-rmi-registry` passed (`RMI_REGISTRY:PASS ids=32 roots=1 session_cleanup=1`); `./tools/build arm` and `./tools/build-pc` passed. RISC-V build, physical APDUs, and flashing were intentionally skipped under ARM-only/no-flash scope.
- Result: Reader access is ready for parallel physical tests. The synthetic RMI test validates the currently modeled remote-info tail only; complete CAP 2.2 parsing and interoperability are not established. Service CAP header bytes identify version 2.1 (`minor=1`, `major=2`).
- Decision: Obtain or generate a remote-enabled CAP 2.2+ fixture and validate complete Header/Class parsing before implementing RMI SELECT reference encoding. Do not infer names from Debug absence or claim interoperability from the synthetic test.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/build-arm-rmi-cap-metadata-resume.log`, `artifacts/logs/build-pc-rmi-cap-metadata-resume.log`; no new physical-test log.


## 2026-10-22 — Run JC305 deploy areas concurrently across three Pico readers

- Step/experiment: `three-reader-core-crypto-service-rerun-20261022-020215`
- Change: Refreshed PC/SC permissions and ran one independent deploy API area per physical Pico using the parallel runner. No firmware was flashed.
- Validation: Elevated `pcsc_scan -r` listed physical readers D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas Core,Crypto,Service --min-devices 3 --max-devices 3` assigned Crypto/Core/Service to separate readers. The run completed in about 59 seconds. Crypto: 466 calls (11 OK, 434 spec-allowed unsupported, 16 EXC_RUNTIME, 3 EXC_CARDRUNTIME, 2 SW_ERROR); cleanup passed. Core: 291 calls (272 OK, 15 spec-allowed unsupported, four optional Ext methods unavailable); optional Ext INSTALL returned 6A88 and rollback passed. Service: 124 calls (94 OK, 28 WRONG_RESULT, one SW_ERROR, one EXC_RUNTIME); cleanup passed. All worker processes exited 0; coordinator exit 1 because optional Ext INSTALL refusal is counted as a failed stage.
- Result: Three-reader routing and parallel deploy testing work again. Results apply to firmware already installed on the cards and do not validate current source. No sequential baseline was collected, so no speedup claim is made.
- Decision: Keep this runner for independent deploy areas; focus implementation work on the RMI metadata gap using a CAP 2.2+ fixture.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-020215/`; CSV SHA-256: Crypto `afc1948e5741f4d7d466e724fc2980fc15f800acc4288d98dff4be81881d35f5`, Core `62ee65f3180cc7f2f52d80182cf5ec93046ea038324589f194094ba4d50ff92e`, Service `ee08d099e0c47a46e8e20ba76ebb2ac8ee33300748e223efe36bb1594ed13fcb`.


## 2026-10-22 — Verify recovery when install-journal cleanup sync fails

- Step/experiment: `install-journal-clear-sync-process-restart`
- Change: Added a PC-only one-shot sync failure, armed specifically during transactional journal cleanup after CAP instance publication. Added a VPCD process-restart test that confirms rollback preserves the journal and a fresh process clears it idempotently. The runner captures the dynamic package ID from CAP LOAD for restart.
- Validation: Initial test run failed SELECT `6F00` because restart used requested package ID 63 instead of assigned ID 56; the failure established the actual package mapping requirement. After correcting package-ID propagation, `./tools/test-pc-vpcd-applet --verify-install-journal-clear-restart --passes 1` passed with `TEST:SUMMARY pass=17 fail=0`, including SELECT `9000`, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 support/doFinal/static, and expected unsupported-algorithm status. `./tools/test-persistent-transaction`, `./tools/build arm`, and `./tools/build-pc` passed. No RISC-V build, physical APDU, or flash was performed.
- Result: Host transaction rollback after journal-clear sync failure leaves a recoverable install journal; fresh-process recovery tolerates an already-published applet instance. This does not prove real RP2350 flash interruption behavior.
- Decision: Extend RP2350 filesystem-overlay cut tests to exercise the journal clear/update boundary. Keep the ARM-only/no-flash restriction.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/install-journal-clear-sync-restart-20261022-retry.log`, `artifacts/logs/build-arm-install-journal-clear-recovery.log`, and `artifacts/logs/build-pc-install-journal-clear-recovery.log`.


## 2026-10-22 — Test install recovery across filesystem and CAP registry cuts

- Step/experiment: `rp2350-overlay-cross-store-install-recovery`
- Change: Extended the RP2350 fake-flash overlay harness with a combined install journal/CAP registry scenario. It commits the filesystem recovery record before applet instance publication, then injects cuts during the actual CAP slot copy-on-write header update and runs idempotent recovery.
- Validation: `./tools/test-rp2350-fs-overlay` passed (`TEST:SUMMARY fail=0`). Across 1,000 deterministic pseudo-random CAP header cut locations, every restart retained an intact journal and a readable old or fully published CAP header; recovery finished instance publication, cleared the journal, and preserved applet AID, instance AID, and class index `0x0123` after a further restart. ARM/host builds were not rerun because only the host fake-flash test harness changed in this step. No RISC-V build, physical APDU, or flash was performed.
- Result: The install ordering across the RP2350 filesystem snapshot and CAP copy-on-write header now has combined restart/failure-injection coverage. This remains fake-flash simulation, not electrical power-cut validation.
- Decision: Continue the broader install/recovery edge audit, then return to Service RMI CAP 2.2 metadata and SELECT/INVOKE work.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/tests/rp2350-fs-overlay/rp2350_fs_overlay_test`; no separate log file.


## 2026-10-22 — Simulate flash cuts while clearing the install journal

- Step/experiment: `rp2350-overlay-install-journal-clear-cuts`
- Change: Extended the RP2350 filesystem-overlay fake-flash test with the install lifecycle journal tag (`02 FF 38 03`). It commits a journal record, stages its transactional erase, then cuts power during the new snapshot write.
- Validation: `./tools/test-rp2350-fs-overlay` passed with `TEST:SUMMARY fail=0`. The targeted test ran 1,000 deterministic pseudo-random cut locations across snapshot erase/program mutations and verified every reboot recovered either the byte-identical committed journal or the committed cleared state. No RISC-V build, physical APDU, or flash was performed.
- Result: Filesystem snapshot atomicity is now directly covered for the install-journal clear operation in the RP2350 storage implementation's fake-flash harness. This does not test actual electrical power cuts or the full CAP-header/filesystem transaction as a single integrated crash fixture.
- Decision: Audit cross-store ordering and add a combined CAP-header/filesystem restart case where the existing host emulation provides meaningful coverage. Keep ARM-only/no-flash scope.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/tests/rp2350-fs-overlay/rp2350_fs_overlay_test`; no separate log file.


## 2026-10-22 — Test atomic install-journal and applet-state creation

- Step/experiment: `rp2350-overlay-install-journal-create-cuts`
- Change: Extended the RP2350 fake-flash overlay harness to create the install intent journal and applet persistent-state record in the same transaction, then inject interruption during the committed snapshot write.
- Validation: `./tools/test-rp2350-fs-overlay` passed with `TEST:SUMMARY fail=0`. The new case ran 1,000 deterministic pseudo-random mutation cut locations; each simulated reboot recovered either neither record or both complete byte-exact records. Existing clear-journal and CAP-header publication cut cases also passed. No ARM build was needed because only the host fake-flash test changed. No RISC-V build, physical APDU, or flash was performed.
- Result: Fake-flash cut coverage now exercises install transaction creation, CAP instance-header publication, and journal clear. It does not establish behavior under electrical power interruption on RP2350 hardware.
- Decision: Continue auditing install crash boundaries, then resume Service RMI fixture work. Keep ARM-only/no-flash scope.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/tests/rp2350-fs-overlay/rp2350_fs_overlay_test`; no separate log file.


## 2026-10-22 — Run all JC305 deploy areas across three physical Picos

- Step/experiment: `three-reader-all-api-areas-20261022-021536`
- Change: Used the three live physical PC/SC readers to run the full JC305 deploy-area set concurrently. The documented PC/SC helper granted access to three USB nodes; `pcsc_scan -r` showed readers D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437 (Java reader indices 3–5). No firmware was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --min-devices 3 --max-devices 3` completed all three workers. Security: 901 calls (63 OK, 820 spec-allowed unsupported, 13 EXC_RUNTIME, two WRONG_RESULT, two user-skipped, one SW_ERROR). Crypto: 466 calls (11 OK, 434 spec-allowed unsupported, 16 EXC_RUNTIME, three EXC_CARDRUNTIME, two SW_ERROR). Service: 124 calls (94 OK, 28 WRONG_RESULT, one EXC_RUNTIME, one SW_ERROR). Core: 291 calls (271 OK, 15 spec-allowed unsupported, four optional Ext scenarios not installed, one WRONG_RESULT). Main CAP cleanup passed. Optional Core Ext, JavacardxSec, Javacardx, JavacardxExt, JavacardxInt, and Biometry INSTALL returned `6A88`; all partial LOADs were rolled back. Workers exited 0; coordinator returned 1 because optional INSTALL refusals are classified as failed stages.
- Result: Three-reader full-area routing and cleanup work. `UNSUPPORTED_PER_SPEC` rows and optional package `6A88` refusals are expected capability/package findings; the runtime, wrong-result, and SW_ERROR rows need current-source correlation before attributing them to code, since this firmware was not built from current source.
- Decision: Validate physical error rows against current-source VPCD coverage before implementation changes, then continue the RMI CAP 2.2+ fixture path. Keep ARM-only/no-flash scope.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-021536/`; CSV SHA-256: pico1 `422ca434884bde410d68982d2037d85bedeb2d91bbc633559b00425731849cfe`, pico2 `e74fea8db046a76e8f36cb47aff697f47b136498debe7963fa8642dcaf4c6e20`, pico3 `08764a6544ed6ce84398552a792845e2d3178404f37e18797549eb8c071a3247`.


## 2026-10-22 — Parse bounded package names from CAP 2.2 Headers

- Step/experiment: `rmi-cap22-header-package-name`
- Change: Added accessors for the CAP 2.2+ Header package-name field following package AID data. Accessors check the Header component length, AID length, name-length byte availability, and complete name bytes before returning a pointer. CAP 2.1 does not expose this optional field.
- Validation: `./tools/test-rmi-cap-metadata` passed synthetic valid CAP 2.2, absent CAP 2.1, truncated-length, and truncated-name cases (`RMI_CAP_METADATA:PASS package_name=bounded methods=1 class_name=Impl interfaces=1`). `./tools/build arm` passed (`BUILD:PASS target=choupi`, image end `0x100c7368`). The initial truncated-name assertion incorrectly expected the still-present length byte to be unavailable; after correcting the test to check full-name rejection, it passed. No RISC-V or PC build, physical APDU, or flash was run.
- Result: CAP package names can now be read safely from a version 2.2+ Header when the component data is complete. This remains synthetic validation; no remote-enabled CAP fixture was checked.
- Decision: Inspect the bundled CAP converter and generated component files for a way to create a small authentic remote-enabled CAP 2.2 fixture, then validate interface references and signature-pool layout.
- Commit: Not created; repository `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/tests/rmi-cap-metadata/rmi_cap_metadata_test`; ARM output `build/arm/pico2_choupi.uf2`; no separate log file.
## 2026-10-22 — Validate CAP 2.2 remote metadata against converter output

- Step/experiment: `rmi-cap22-component-layout-fixture`
- Change: Added CAP Class component accessors for the CAP 2.2+ signature-pool
  prefix and optional remote interface names, including rejection of a
  signature-pool length that exceeds the declared component. Extended the RMI
  metadata test to parse exact Header/Class component bytes emitted by the
  bundled JCDK 3.0.5 converter for a remote-enabled CAP 2.2 fixture. Updated
  TODO, QKB, and handoff with the new evidence and remaining RMI work.
- Validation: `./tools/test-rmi-cap-metadata` passed:
  `RMI_CAP_METADATA:PASS package_name=bounded signature_pool=4
  remote_methods=2 interface=Counter class=CounterImpl`. `./tools/build arm`
  passed (`BUILD:PASS target=choupi`, image end `0x100c7368`). The authorized
  host USB path refreshed access and elevated `pcsc_scan -r` showed all three
  physical readers. RISC-V/PC builds, physical APDUs, and flashing were
  intentionally skipped. `git diff --check` reports CR at end-of-line as
  whitespace on the repository's CRLF-formatted modified source; no compiler
  warnings were reported.
- Result: Actual emitted CAP 2.2 component layout is validated for the package
  name, signature pool, remote interface/class metadata, method hashes and
  remote-interface references. The CAP itself has not been loaded/executed;
  SELECT/INVOKE interoperability remains unverified.
- Decision: Connect the parsed CAP metadata to RMI SELECT FCI/reference
  encoding, then run a current-source VPCD scenario with this fixture. Keep
  ARM-only/no-flash scope.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/rmi-cap22-fixture/cap/rmi/fixture/javacard/fixture.cap`
  (SHA-256 `5ef8e4587a9445bf5bc80b25b36496261c49e18e5d18e87039254d53a5eb114c`);
  test binary `build/tests/rmi-cap-metadata/rmi_cap_metadata_test`;
  firmware image `build/arm/pico2_choupi.uf2`.

## 2026-10-22 — Run JC305 deploy APIs across three physical readers

- Step/experiment: `three-reader-deploy-all-areas-20261022-023306`
- Change: Ran the deploy-folder JC305 API test runner across all three attached
  Pico PC/SC readers concurrently. The run performed CAP install/test/cleanup;
  no firmware was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3` found D92E743D5F640C0F,
  E0E14525E64AF6ED, and 5CFC5328F5BDE437. Security: 901 calls (63 OK, 820
  per-spec unsupported, 13 EXC_RUNTIME, two WRONG_RESULT, two skipped, one
  SW_ERROR). Crypto: 466 (11 OK, 434 per-spec unsupported, 16 EXC_RUNTIME,
  three EXC_CARDRUNTIME, two SW_ERROR). Service: 124 (94 OK, 28 WRONG_RESULT,
  one EXC_RUNTIME, one SW_ERROR). Core: 291 (272 OK, 15 per-spec unsupported,
  four optional scenarios unavailable). Optional JavacardxSec, Javacardx,
  JavacardxExt, JavacardxInt, Biometry, and Core Ext INSTALLs returned `6A88`;
  each partial load was rolled back. Workers exited 0; coordinator exited 1
  because those six optional install stages are classified as failures.
- Result: Three physical readers were successfully used in parallel for the
  full JC305 deploy set. Unsupported-per-spec results and missing optional
  packages are expected profile findings. Runtime/wrong-result/SW_ERROR rows
  describe firmware already installed on the boards and do not validate current
  source.
- Decision: Keep this as the physical deploy baseline. Continue current-source
  RMI implementation through VPCD; do not flash under ARM-only scope and the
  two-architecture flash gate.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-023306/`; CSV
  SHA-256: pico1 `ded2b44d0597a915c44678516f7e74fcfe13bb70a55d561212ec7297b171d8ad`,
  pico2 `f5343711edf7224d227e25fdb34220bb83ed401322fefce613a221c465b95308`,
  pico3 `ad2952ff180025e77b4c66ea67c91becb1158604a75244027b12cdafafb987df`.

## 2026-10-22 — Encode standard Java Card RMI reference descriptors

- Step/experiment: `rmi-reference-descriptor-encoders`
- Change: Added bounded encoders for both `remote_ref_with_class` and
  `remote_ref_with_interfaces` descriptors. They serialize the 16-bit object
  ID, hash modifier, package/class or interface names, and validate the
  reserved null ID, interface count, required names, and destination capacity.
  Extended the converter-emitted CAP fixture test with descriptor
  known-answer assertions.
- Validation: `./tools/test-rmi-cap-metadata` passed with
  `RMI_CAP_METADATA:PASS package_name=bounded signature_pool=4
  remote_methods=2 interface=Counter class=CounterImpl`; class and interface
  descriptor bytes matched the expected Java Card RMI encodings. Rejected
  `0xFFFF`, missing first interface package name, and undersized output buffer.
  `./tools/build arm` passed (`BUILD:PASS target=choupi`, image end
  `0x100c7368`). No RISC-V or PC build, physical APDU, or flash was run.
- Result: The RMI descriptor serialization format is covered by fixture-backed
  known-answer checks, but no runtime/native code calls these encoders yet.
- Decision: Add the native bridge from exported object to CAP remote metadata,
  then generate FCI in `RMIService.processCommand()` and verify SELECT through
  current-source VPCD.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `tools/tests/rmi_cap_metadata/rmi_cap_metadata_test.cpp`;
  `build/tests/rmi-cap-metadata/rmi_cap_metadata_test`;
  `build/arm/pico2_choupi.uf2`.

## 2026-10-22 — Revalidate parallel deploy across three connected Pico 2 readers

- Step/experiment: `three-reader-deploy-all-areas-20261022-024604`
- Change: Re-ran the Java Card 3.0.5 deploy suite with the installer helper's
  three physical readers. No firmware was flashed.
- Validation: `PICO2_HOST_USB_ACCESS=1 sudo -n
  /usr/local/sbin/pico2-pcsc-reader && PICO2_HOST_USB_ACCESS=1 pcsc_scan -r`
  found D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. The parallel
  deploy runner found three readers and launched three workers. Security: 901
  calls (63 OK, 820 `UNSUPPORTED_PER_SPEC`, 13 `EXC_RUNTIME`, two
  `WRONG_RESULT`, two skipped, one `SW_ERROR`). Crypto: 466 calls (11 OK, 434
  `UNSUPPORTED_PER_SPEC`, 16 `EXC_RUNTIME`, three `EXC_CARDRUNTIME`, two
  `SW_ERROR`). Service: 124 calls (94 OK, 28 `WRONG_RESULT`, one
  `EXC_RUNTIME`, one `SW_ERROR`). Core: 291 calls (272 OK, 15
  `UNSUPPORTED_PER_SPEC`, four optional scenarios unavailable). Six optional
  INSTALL stages returned `6A88`; all partial loads were rolled back. All
  workers exited 0; coordinator exited 1 because of these optional stages.
- Result: Three attached readers were used concurrently. Results exercise the
  previously installed firmware and do not validate current source.
- Decision: Keep all three devices available for parallel testing. Continue
  current-source RMI work in the host emulator; no flash is authorized under
  the ARM-only/two-architecture flash gate.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-024604/`.

## 2026-10-22 — Implement RMI SELECT FCI for CAP 2.2 remote objects

- Step/experiment: `rmi-select-fci-vpcd-20261022`
- Change: Wired the CAP remote-metadata encoders into a native
  `RMIService.processCommand()` SELECT path, emitted the Java Card RMI FCI
  structure, restored the initial object's transient export registration when
  the installed applet graph is restored, and accepted SELECT P2 `0x10` in the
  CCID/VPCD applet-selection paths. The VPCD harness can request either P2
  form and assert exact SELECT response data.
- Validation: `./tools/test-rmi-cap-metadata` passed. `./tools/build arm`
  passed (`BUILD:PASS target=choupi`, image end `0x100c794c`), and
  `./tools/build-pc` passed (`ROMMASK:PASS`, host emulator linked). VPCD
  fixture SELECT passed with exact expected FCI for class form
  (`6F236E215E1F020238810001000B726D692F666978747572650B436F756E746572496D706C`)
  and interface form
  (`6F206E1E5E1C02023881000100010B726D692F6669787475726507436F756E746572`);
  both report `TEST:SUMMARY pass=2 fail=0`. Harness Python syntax compilation
  passed. `git diff --check` still reports CR characters in pre-existing
  CRLF-formatted modified source files; line endings were preserved.
- Result: The VM installs the converter-generated CAP 2.2 fixture, selects its
  applet, retains/restores the initial remote object, and returns the expected
  class or interface reference in FCI. No external JCRMI client or remote
  INVOKE was tested. No firmware was flashed.
- Decision: Continue with RMI INVOKE command parsing, remote method lookup,
  parameter decoding, dispatch, and response marshalling. Preserve ARM-only
  and no-flash scope.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/rmi-select-class-vpcd-20261022.log`,
  `artifacts/logs/rmi-select-interfaces-vpcd-20261022.log`, fixture
  `build/rmi-cap22-fixture/cap/rmi/fixture/javacard/fixture.cap`.
## 2026-10-22 — Three-reader deploy and persistent RMI primitive fields

- Step/experiment: `three-reader-deploy-rmi-short-invoke-20261022`
- Change: Confirmed three distinct physical readers are routable concurrently.
  Fixed persistent byte, short, and int getters to read through the restored
  instance field cache, preserving default values and type validation. Removed
  temporary RMI/frame/field tracing after diagnosis.
- Validation: Reader dry run mapped D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437 to indices 3–5. Parallel JC305 run of Crypto/Core/Service
  completed with all three workers exit 0; only optional Core Ext INSTALL was
  refused with expected `6A88` and rolled back. Final `pcsc_scan -r` listed all
  three readers. `./tools/build arm`, `./tools/build riscv`, and
  `./tools/build-pc` passed. Current-source VPCD SELECT + `Counter.add(short)`
  + `Counter.value()` passed exactly (`8100039000` for both method replies),
  `TEST:SUMMARY pass=6 fail=0`, including unknown method and object error
  responses. `git diff --check` still flags CRLF lines in
  modified CRLF source files; line endings were preserved.
- Result: Three-reader API workers completed in parallel in about one minute.
  Crypto: 466 calls (11 OK, 434 expected unsupported, 16 runtime exceptions,
  3 card-runtime exceptions, 2 SW_ERROR); Core: 291 (271 OK, 15 expected
  unsupported, 4 optional applet unavailable, 1 wrong result); Service: 124
  (94 OK, 28 wrong results, 1 runtime exception, 1 SW_ERROR). These cards had
  existing firmware; no flash was performed. RMI short field mutation and
  readback now work in current-source VPCD.
- Decision: Keep the three-reader runner using one independent area per reader
  for stable parallel coverage. Continue RMI reference marshalling and broader
  persistent-field tests. Do not report current-source physical validation.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-032759/`
  (CSV SHA-256: pico1 `4ee6b58aa9f63bd6fa537320a456a8bd0561557dda159357f559076f43a6515b`,
  pico2 `325789010ddea6639ee78920ece89bf14c1886f87c48b765ba11720c424423f1`,
  pico3 `a00883dbf623f1a3308c73468276771b4d12c773132a79e63f04e6232ab538a4`);
  `artifacts/logs/rmi-invoke-primitives-errors-20261022.log`.

# 2026-10-22 — Retest deploy API areas across three physical readers

- Step/experiment: `deploy-three-reader-full-20261022-034350`
- Change: Refreshed physical test baseline across the three connected Pico 2
  readers using the repository's parallel API deploy runner. Removed only the
  stale Security and JavacardxInt deploy CAPs from the exact cards that
  reported duplicate packages; reran both areas against their pinned readers.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3` found all three readers. Core (291 calls),
  Crypto (466), and Service (124) completed and cleaned up. Security rerun
  completed 901 calls and cleaned up. JavacardxInt `INSTALL` returned `6A88`
  for an unavailable import and rollback passed. Other optional package
  installs were refused with `6A88` and rolled back. No build or flash was
  performed; this validates the already-installed firmware only.
- Result: All three readers are usable. Security reproduced two wrong results,
  13 runtime exceptions, and one SW_ERROR; Core reproduced one
  `Applet.deselect()` wrong result; Service reproduced 28 wrong results, one
  runtime exception, and one SW_ERROR. Unsupported-per-spec calls remain
  expected. The stale-package cause and exact-reader cleanup are in QKB.
- Decision: Keep all three-reader routing; clean stale CAPs by exact reader
  before rerunning an interrupted area. Continue with current-source deploy
  applet tests and investigate reproducible Core/runtime findings. No physical
  validation of the changed source is claimed.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-034350/`.

# 2026-10-22 — Add Java Card RMI primitive-array marshalling

- Step/experiment: `rmi-array-invoke-vpcd-20261022`
- Change: Implemented CAP 2.2 RMI decoding for boolean[], byte[], and short[]
  input arrays and by-value array/null-array response encoding. Incoming arrays
  use JCRE global arrays and are released after invocation; global heap slots
  remain stable and are recycled. Added a Java Card 3.0.5 CAP fixture builder,
  a Class component remote-method inspector, and a repeatable VPCD test wrapper.
- Validation: `./tools/test-rmi-arrays-vpcd` passed `./tools/build arm`,
  `./tools/build-pc`, CAP fixture conversion, and current-source VPCD with
  `TEST:SUMMARY pass=13 fail=0`. Tests include primitive add/readback,
  byte[] echo/sum, short[] echo, boolean[] mutation, eight array parameters,
  null/empty arrays, truncated array input, and invalid boolean values. ARM
  output was `BUILD:PASS target=choupi`; PC output was `ROMMASK:PASS`. The PC
  build reported 49 warnings from the bundled `javacard-os` Rust crate.
  RISC-V compilation and physical flashing/testing were intentionally skipped
  under the user's ARM-only constraint.
- Result: Exact responses passed for byte[] (`810211229000`), short (`8100079000`),
  eight arrays (`8100249000`), short[] (`81021234FFFE9000`), boolean[]
  (`810201009000`), null (`81FFFF9000`), empty (`81009000`), and malformed
  inputs (`9900039000`). Int[] code is guarded by `JCVM_INT_SUPPORTED` but has
  no end-to-end fixture coverage yet.
- Decision: Keep RMI array-by-value support. Next implement remote-reference
  return and Java Card API exception response encoding, then external JCRMI
  interoperability. No current-source physical test is claimed.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `tools/tests/rmi_array_fixture/`,
  `tools/build-rmi-array-fixture`, `tools/test-rmi-arrays-vpcd`, fixture CAP
  SHA-256 `5f7c5301002fa8d2f8a73d9837a3bed230737042bd982e16b79b9b01b756479f`,
  load-code SHA-256 `f8d760eba9e1a8cbe6368847bf897ff2b421e1de4c1d0d7c92bf025b4be0cfa`,
  and `artifacts/logs/rmi-array-invoke-vpcd-20261022T015928Z.log`.

## 2026-10-22 — Reconfirm three-reader deploy coverage

- Step/experiment: `deploy-three-reader-reconfirm-20261022-040147`
- Change: Reused the refreshed PC/SC setup to run the API 3.0.5 deploy suite
  across three physical Pico readers concurrently. No firmware was flashed;
  this tested the already-installed card images.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --min-devices 3 --max-devices 3` completed all three workers. A post-run
  elevated dry run again found D92E743D5F640C0F, E0E14525E64AF6ED, and
  5CFC5328F5BDE437 at Java reader indices 3–5. Security: 901 calls (63 OK,
  820 unsupported per spec, 13 runtime exceptions, two skipped, two wrong
  results, one SW_ERROR). Crypto: 466 (11 OK, 434 unsupported per spec, 16
  runtime exceptions, three card-runtime exceptions, two SW_ERROR). Core: 291
  (270 OK, 15 unsupported per spec, four optional applets unavailable, two
  wrong results). Service: 124 (94 OK, 28 wrong results, one runtime
  exception, one SW_ERROR). All supported-area upload, test, cleanup stages
  passed. Optional Javacardx-family/Biometry installs returned `6A88` and
  their partial loads were rolled back. No build or flash was performed.
- Result: Three-reader routing and cleanup work. `UNSUPPORTED_PER_SPEC` and
  missing optional imported packages are expected profile limits; the
  reproducible wrong/runtime results remain follow-up items. These findings
  are from existing card firmware, not current source.
- Decision: Keep the three-device parallel setup available for physical
  validation. Continue current-source RMI support via VPCD; physical flashing
  remains gated by the required dual-architecture build.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-040147/`.

## 2026-10-22 — Encode Java Card exceptions in RMI INVOKE responses

- Step/experiment: `rmi-exception-response-vpcd-20261022`
- Change: Added CAP 2.2 `0x82` exception response encoding for VM-raised API
  exception types and reasons. The nested remote invocation now bounds Java
  exception-handler search at its entry frame, still honors handlers inside
  the invoked method, and recognizes ISOException before returning an uncaught
  exception. This keeps a remote failure from unwinding into RMIService's own
  Java frames.
- Validation: `./tools/test-rmi-arrays-vpcd` passed ARM, PC, fixture build, and
  current-source VPCD with `TEST:SUMMARY pass=18 fail=0`. Exact responses
  passed for ArrayIndexOutOfBoundsException (`820200009000`),
  NullPointerException (`820800009000`), SystemException.ILLEGAL_VALUE
  (`822500019000`), ISOException `0x6A82` (`82236A829000`), and a caught
  exception method (`81559000`). JCSystem VPCD cases 19, 20, 21, 22, and 24
  passed. No RISC-V build or physical flash was run under the ARM-only gate.
- Result: Unhandled API exceptions now use JCRMI exception responses instead
  of generic `6F00`; the nested invoke boundary preserves local handlers.
  Broader JCSystem regression remains red: full run case 5 expected persistent
  field `0x11` after abort but got `0x22`; selected case 27 left the
  APDUException sentinel `0x7F` instead of its expected reason. Causes are not
  established and are recorded in `.agent/QKB.md`. Custom exception subclass
  `0x83` responses and complete API reason capture remain open.
- Decision: Keep this exception boundary specific to RMI invocation. Continue
  with remote-reference return values and then external JCRMI interoperability.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261022T021751Z.log`;
  fixture CAP SHA-256 `972ce3969de0353d45395c63f7682b2fd2b845bfa8748d2375ce0b6b4fb201bb`;
  load code SHA-256 `f3f8cf42cdb6ec449979aeac7f1b467360c49ddaf21660a4b79b2fb1d8863617`.


## 2026-10-22 — Marshal Java Card RMI remote-reference returns

- Step/experiment: `rmi-remote-reference-returns-vpcd-20261022`
- Change: Decode CAP 2.2 remote-interface return signatures, validate the returned object's declared interface, and encode exported references in the selected class or interface form. Returned objects are retained for the active RMI session. Expanded the CAP fixture so its initial `Counter` returns a newly exported child and added a follow-up invocation through the returned object reference.
- Validation: `./tools/test-rmi-arrays-vpcd` passed ARM and PC builds and VPCD tests. Class form returned `810002000B726D692F666978747572650B436F756E746572496D706C9000`; the client then invoked `value()` on object ID `0002` and received `8100009000`. Interface form returned `81000200010B726D692F6669787475726507436F756E7465729000`. Results: `TEST:SUMMARY pass=20 fail=0` and `TEST:SUMMARY pass=3 fail=0`. The VPCD test needed elevated execution because restricted local socket creation returned `EPERM`. No USB, physical token, RISC-V build, or flash was used under the current ARM-only/no-flash gate.
- Result: Remote interface returns can now be called through both supported descriptor forms in the emulator fixture. Fixture CAP SHA-256 `bf9edd40addde9adb680fa3f691f9d9d00f1accdfc01c2d2835af6047bd4b400`; load-code SHA-256 `3baf35b6c1378d95332e10b39ba1931414d542444ba4529081ed607a7ab09f7a`.
- Decision: Continue with custom Java exception subclasses and external JCRMI client interoperability. Physical validation remains gated by the two-architecture build requirement.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261022T022852Z.log`, `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T022852Z.log`, `artifacts/logs/build-arm-rmi-arrays-20261022T022852Z.log`, `artifacts/logs/build-pc-rmi-arrays-20261022T022852Z.log`.

## 2026-10-22 — Investigate end-to-end RMI int-array fixture coverage

- Step/experiment: `rmi-int-array-fixture-converter-investigation`
- Change: Attempted to extend the CAP 2.2 fixture with `int[]` parameter and return methods. The stock `ant-javacard` task has no int-support attribute, so tested a direct JCDK 3.0.5 converter invocation with `-i` enabled. The converter accepted the package but its generated `Class.cap` remote-method table still omitted both methods. Restored the existing fixture source and build workflow; the baseline fixture again builds and lists the expected current signatures.
- Validation: `./tools/build-rmi-array-fixture` passes after restoration. Before restoration, the converter output was inspected with `build/tests/rmi-array-fixture/rmi_array_cap_inspect`; no int signatures were emitted, so no int-array APDU assertions were attempted. No ARM build, RISC-V build, VPCD exchange, USB access, or hardware operation was needed for this converter investigation.
- Result: Existing guarded int-array RMI code remains unverified end to end. The converter limitation is reproducible, but its cause is not established. Details are in `.agent/QKB.md`.
- Decision: Continue with other RMI/deploy coverage; revisit int[] once a valid CAP 2.2 fixture can be generated.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/rmi-array-fixture/build.log`, `build/rmi-array-fixture/remote-methods.txt`, and `build/rmi-array-fixture/Class.cap`.

## 2026-10-22 — Restore baseline after int RMI fixture experiment

- Step/experiment: `rmi-int-array-fixture-baseline-restored`
- Change: Restored the fixture source and JCDK Ant task after the optional-int converter experiment; no runtime or production source was changed by that experiment.
- Validation: `./tools/build-rmi-array-fixture` passed and emitted the baseline method list, including the remote-reference return method. It still contains no int[] RMI methods; see the preceding investigation record and `.agent/QKB.md`.
- Result: The existing remote-array fixture remains usable for byte, short, and boolean tests; int[] RMI is explicitly unverified.
- Decision: Continue implementation and deploy-app test coverage on other supported paths while keeping this converter gap open.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `build/rmi-array-fixture/build.log`, `build/rmi-array-fixture/remote-methods.txt`, and `build/rmi-array-fixture/Class.cap`.

## 2026-10-22 — Encode RMI custom exception subclasses

- Step/experiment: `rmi-custom-exception-subclass-vpcd-20261022`
- Change: Preserve the uncaught Java exception object reference at the remote
  invocation boundary. Walk its class hierarchy to find the nearest exported
  Java Card API exception, and return CAP 2.2 exception-subclass tag `0x83`
  for a user-defined subclass. Added `CustomRuntimeException` to the RMI CAP
  fixture.
- Validation: `./tools/test-rmi-arrays-vpcd` passed ARM and host builds,
  fixture generation, current-source VPCD (`TEST:SUMMARY pass=21 fail=0`), and
  interface-reference VPCD (`TEST:SUMMARY pass=3 fail=0`). The new test returns
  exactly `830900009000` for `CustomRuntimeException extends RuntimeException`;
  all existing API exception and array cases remained green. Host VPCD was run
  with approved escalated local socket access. No RISC-V build, USB/PCSC access,
  physical APDU, or flash was performed.
- Result: Custom runtime exception instances now encode as type RuntimeException
  (0x09), reason zero, subclass tag 0x83. Other custom class hierarchies and
  complete exception reason coverage remain unverified.
- Decision: Continue with external JCRMI client interoperability and broader
  exception hierarchy coverage. Current-source hardware validation remains
  under the ARM-only/no-flash constraint.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261022T024228Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T024228Z.log`,
  `artifacts/logs/build-arm-rmi-arrays-20261022T024228Z.log`,
  `artifacts/logs/build-pc-rmi-arrays-20261022T024228Z.log`; fixture CAP SHA-256
  `0289e897fbeb1d6dbcee223a4f6ddd51b6ae562a53cea86067338c8156d42398`, load
  code SHA-256
  `e9f4b3cd291feb670714e6b215257638981f3fd75e41a57de8261d00f9a15373`.

## 2026-10-22 — Cover checked RMI exception subclasses

- Step/experiment: `rmi-checked-exception-subclass-vpcd-20261022`
- Change: Added a checked custom exception extending `java.lang.Exception` and
  a remote fixture method that throws it. Added an exact response assertion to
  the VPCD invocation suite, complementing the runtime-subclass case.
- Validation: `./tools/test-rmi-arrays-vpcd` passed ARM and host builds,
  fixture conversion, VPCD invocation (`TEST:SUMMARY pass=22 fail=0`), and
  interface reference tests (`TEST:SUMMARY pass=3 fail=0`). Exact custom
  responses: RuntimeException subclass `830900009000`; Exception subclass
  `830500009000`. Host VPCD used escalated local socket access. No RISC-V,
  physical APDU, USB, or flash was run.
- Result: Both custom class hierarchies return the nearest API type with
  subclass tag `0x83` and zero reason.
- Decision: Continue with an external JCRMI client exchange and broader reason
  code coverage. Hardware flashing remains gated by the two-architecture build
  requirement; ARM-only scope is still active.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261022T024418Z.log`,
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T024418Z.log`,
  `artifacts/logs/build-arm-rmi-arrays-20261022T024418Z.log`,
  `artifacts/logs/build-pc-rmi-arrays-20261022T024418Z.log`; fixture CAP SHA-256
  `cdbb5be061b06d7b61338cc28f2f797947714dbc9ca23a21a8e70454007e66ed`, load
  code SHA-256
  `e3ef992a2ed5db38c468db02e74787f774666f24866e13b39644ce4d0e8b196a`.

## 2026-10-22 — Run JC305 areas across three physical readers

- Step/experiment: `deploy-three-reader-refresh-20261022-044709`
- Change: Assigned one JC305 deploy API area to each refreshed PC/SC reader and
  ran Core, Crypto, and Service concurrently. No firmware was flashed; tests
  exercised the images already installed on the cards.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305
  --areas Core,Crypto,Service --min-devices 3 --max-devices 3` detected all
  three readers and completed all workers. Crypto on D92E743D5F640C0F: 466
  calls (11 OK, 434 unsupported per spec, 16 runtime exceptions, 3 card-runtime
  exceptions, 2 status errors); upload and cleanup passed. Core on
  E0E14525E64AF6ED: 291 calls (271 OK, 15 unsupported per spec, 4 optional
  applets unavailable, 1 wrong result); primary upload and cleanup passed.
  Service on 5CFC5328F5BDE437: 124 calls (94 OK, 28 wrong results, 1 status
  error, 1 runtime exception); upload and cleanup passed. Optional Core Ext
  INSTALL returned `6A88`; its CAP rollback passed. The coordinator returned
  1 for this optional refused stage, while each worker returned 0.
- Result: Three-reader routing, concurrent testing, and cleanup work. The
  optional package refusal is expected; persistent Core/Service wrong results
  and Crypto errors are findings from existing firmware and need later focused
  diagnosis.
- Decision: Keep this parallel setup for physical deploy coverage. Current
  source validation remains in VPCD under ARM-only/no-flash constraints.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-044709/pico1.csv`
  through `pico3.csv` and corresponding `.log` files.

## 2026-10-22 — Exercise the official JCRMI client proxy over VPCD

- Step/experiment: `rmi-official-client-proxy-vpcd-20261022`
- Change: Added a repeatable `tools/test-rmi-client-vpcd` harness around the
  bundled Oracle Java Card 2.2.1 RMI client and proxy factory. It connects over
  the local VPCD protocol, runs the RMI fixture, and reports wire and client
  response encodings.
- Validation: `./tools/test-rmi-client-vpcd` passed host build, fixture build,
  SDK SELECT with the harness's documented Le adaptation, primitive
  `add(short)` (returned 3), and runtime/checked exception proxy decoding.
  Responses were `830900009000` and `830500009000`; the checked exception is
  surfaced as `UndeclaredThrowableException` because the proxy method declares
  only the concrete custom exception type. Without the Le adaptation, SDK
  SELECT with `Le=7F` returns `6D00`. No ARM/RISC-V build, physical APDU, USB,
  or flash was performed in this host-only client step.
- Result: The protocol implementation interworks with the official SDK proxy
  after adapting SELECT Le and the SDK-specific status-first APDU convention.
  Full stock-client selection remains unverified; root cause is unknown and
  documented in `.agent/QKB.md`.
- Decision: Fix or explain SELECT Le handling, then remove the Le adaptation
  and rerun. Continue preserving the runtime/checked subclass tests.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `tools/test-rmi-client-vpcd`,
  `tools/tests/rmi_client_probe/RmiClientProbe.java`,
  `artifacts/logs/build-pc-rmi-client-20261022T025456Z.log`,
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T025456Z.log`,
  CAP SHA-256 `8767df84866fbd257a5064881cda53e49a06548ab5852dc2b21780bab1246da6`,
  load code SHA-256
  `e3ef992a2ed5db38c468db02e74787f774666f24866e13b39644ce4d0e8b196a`.

## 2026-10-22 — Retest three JC305 areas after reader setup refresh

- Step/experiment: `three-reader-core-crypto-service-20261022-050033`
- Change: Reused the three-reader coordinator to run Core, Crypto, and Service concurrently after the helper update. No firmware was flashed; the test CAPs were removed at completion.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --areas Core,Crypto,Service --min-devices 3 --max-devices 3` found three readers. Crypto (D92E743D5F640C0F): 466 calls (11 OK, 434 unsupported per spec, 16 runtime exceptions, 3 card-runtime exceptions, 2 SW errors); upload/cleanup passed. Core (E0E14525E64AF6ED): 291 calls (272 OK, 15 unsupported per spec, 4 optional applets unavailable); Core Ext INSTALL returned `6A88`, and rollback plus both cleanup steps passed. Service (5CFC5328F5BDE437): 124 calls (94 OK, 28 wrong results, 1 runtime exception, 1 SW error); upload/cleanup passed. All workers exited 0; coordinator exit 1 due to the expected optional Ext INSTALL refusal. Post-run `PICO2_HOST_USB_ACCESS=1 pcsc_scan -r` listed all three physical readers.
- Result: Parallel reader routing and cleanup passed again. Physical Crypto and Service findings match the existing firmware's known behavior; they do not validate current source. The current-source Service API catalog has already passed VPCD.
- Decision: Keep the three-reader setup for physical deploy coverage. Continue current-source feature work on VPCD; no new hardware image was flashed.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-050033/`; CSV SHA-256: pico1 `23c30f41cdae7e019b3201f0e5b6330a6adbe890416ef167f3c783eb7caa7917`, pico2 `0415dd6d68c7a3736fd6d50f3070e9cce11bc35fb5af2700532dc4111db95bcf`, pico3 `81eed8912ca2ee90467aed59db28f1d4b7b1fab3766d52be4c955ae0698b728f`.

## 2026-10-22 — Accept SDK RMI SELECT Le=7F

- Step/experiment: `rmi-sdk-select-le-20261022`
- Change: `is_select_for_applet()` now accepts any optional short Le byte after validating the SELECT header, AID length, and APDU length. The official SDK's `Le=7F` had been rejected by an unnecessary equality check against `00`, leaving `NativeImplementation.isAppletSelecting()` false and causing RMIService to ignore the SELECT.
- Validation: `./tools/build arm` passed with `BUILD:PASS arch=arm`; `./tools/build-pc` passed (existing Rust crate warnings and packed-member warnings remain). The unadapted `./tools/test-rmi-client-vpcd` passed through elevated local VPCD socket access: SELECT `00A4041009A000000151000001017F` returned interface FCI `6F206E1E5E1C02023881000100010B726D692F6669787475726507436F756E7465729000`; `Counter.add(short)` returned 3; custom runtime and checked exceptions were decoded. `git diff --check` passed. RISC-V build and physical flash were intentionally skipped under ARM-only/no-flash scope.
- Result: Stock SDK SELECT Le is accepted by current-source VPCD. The SDK wraps the checked exception in `UndeclaredThrowableException` because its remote method declares only the custom subclass while the protocol returns the nearest exported parent type.
- Decision: Mark the short case-4 SELECT Le issue resolved. Proceed with remaining deploy API gaps; next priority is SensitiveResult state and result capture.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/build-pc-rmi-client-20261022T030617Z.log` (SHA-256 `6a4bced5e0c0c63fceb648760633a8f94653760a456f09f4141ae60fb2750371`) and `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T030617Z.log` (SHA-256 `bea638618b3017c2382314ce46ab8fcc881d58fbad678783b8c90a7086bf8d1e`).

## 2026-10-22 — Implement initial VM-wide SensitiveResult state

- Step/experiment: `sensitive-result-vm-wide-20261022`
- Change: Replaced `SensitiveResult` reset/assert stubs with native bridge
  operations and added typed capture for AID equality, JCSystem AID lookup,
  and selected Util copy/fill/compare operations. Object results are GC roots.
  Made the slot VM-wide after checking the API contract: the saved result must
  survive context switches, including Shareable Interface Object calls.
- Validation: `./tools/build arm` passed (`BUILD:PASS target=choupi`);
  `./tools/build-pc` passed (`ROMMASK:PASS`, host target built). No RISC-V build
  or firmware flash was run under the ARM-only/no-flash scope. Current-source
  `JavacardxSec` execution and explicit context-switch coverage are not yet
  validated.
- Result: Both build targets link with the new native methods. A three-reader
  physical deploy run completed Security (901 calls: 63 OK, 820
  `UNSUPPORTED_PER_SPEC`, 13 `EXC_RUNTIME`, 2 `WRONG_RESULT`, 1 `SW_ERROR`, 2
  skipped) and cleanup on ATR `3B800181`. JavacardxSec and JavacardxExt INSTALL
  each returned `6A88`; both partial loads were rolled back. These physical
  results used existing firmware and do not validate current source.
- Decision: Continue with current-source VPCD execution, first covering
  same-context assertions and then result visibility across a context switch.
  Implement the remaining sensitive API result producers before marking the
  JavacardxSec profile complete.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/build-arm-sensitive-result-vm-wide-20261022.log`,
  `artifacts/logs/build-pc-sensitive-result-vm-wide-20261022.log`, and
  `artifacts/results/deploy/parallel-20261022-051734/`. SHA-256: ARM log
  `2e1824b098daa305e69b3e4f6173205a56faad94221d621547b5efec73805b0c`, PC log
  `1231fbdd7c246b03a4cb223d8c9e2597998df0bc8288b3f683000f42d123b8be`, Pico 1
  CSV `673d8fa0157e1382b32df9e7cfcbe2f7798f06a411fc502b4b5c182bbc276a7c`, Pico
  2 CSV `69cf804aa4756e98791ef3863faa6bb259a94ced55bca7e4efae79fe79426543`, Pico
  3 CSV `4e83c3e2687edd54277db9d86a5a56cd3d27c346e279eab7249678bd47a31bfe`.

## 2026-10-22 — Exercise SensitiveResult through the SDK RMI client

- Step/experiment: `sensitive-result-same-context-vpcd-20261022`
- Change: Extended the RMI fixture with a remote method that exercises
  `JCSystem.getAID`, `AID.equals`, `Util.arrayCopyNonAtomic`, and
  `Util.arrayCompare` against `SensitiveResult` object/boolean/short
  assertions. It also checks wrong-value rejection and reset to Unassigned.
- Validation: `./tools/test-rmi-client-vpcd` built the fixture, selected it with
  the unmodified Oracle JCRMI client over VPCD, and reported
  `JCRMI_CLIENT:PASS SensitiveResult same-context assertions`. The emulator log
  ends with `VPCD:DISCONNECTED result=0`. This covers current-source PC VPCD;
  no physical APDU or flash was used.
- Result: Initial result capture, assertion type/value checks, failure
  preservation, and reset are exercised in one applet context. The test does
  not yet exercise an SIO context switch or the full 84-call deploy API area.
- Decision: Keep SensitiveResult active and add cross-context coverage next.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/logs/build-pc-rmi-client-20261022T032550Z.log`
  (SHA-256 `98f3da01f60c2ebc5a40d8c4706f8eddae60542dd1444ad8e3ccccf6d8492add`),
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T032550Z.log`
  (SHA-256 `d94be3a9fe6929c3c0e9b757ee1628c91cd74dda863ecf073db633a0e7354e99`),
  and `build/rmi-array-fixture/`.

## 2026-10-22 — Automate current-source JavacardxSec VPCD validation

- Step/experiment: `javacardxsec-deploy-vpcd-20261022`
- Change: Added `tools/VpcdPresenceProbe.java` and
  `tools/test-javacardxsec-vpcd`. The wrapper checks the elevated host-access
  path, runs a bounded SmartcardIO presence query before emulator startup,
  confirms card presence after connection, runs the official deploy client,
  and requires exactly 84 `OK` call rows.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-javacardxsec-vpcd` passed
  through the escalated host-access path. It reported
  `VPCD:PRESENCE_QUERY ... present=false` before emulator startup,
  `present=true` after connection, and `VPCD_JAVACARDXSEC:PASS calls=84
  status=OK`. The first wrapper attempt had a one-second startup race and was
  rejected by the post-run CSV check; increasing the wait and requiring a
  second presence query made the run deterministic. No physical token was
  accessed or flashed.
- Result: Current-source PC VPCD completes the entire JC305 JavacardxSec area:
  84 calls, all `OK`, including APDUUtil and SensitiveResult. The pre-query is
  required because listing a Virtual PCD reader does not activate its TCP
  listener. SIO context-switch behavior remains outside this suite.
- Decision: Mark the current-source JavacardxSec profile validated in VPCD;
  continue with cross-context JCSystem/SIO behavior and remaining sensitive
  API producers.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `tools/test-javacardxsec-vpcd`,
  `tools/VpcdPresenceProbe.java`, and
  `artifacts/results/javacardxsec-vpcd-20261022T033243Z/`. CSV SHA-256
  `08cc5fe7026075ec876e5795fb4700885423c52eb60b7d5b1be69bf92c1f3e2c`;
  emulator log SHA-256
  `b089d95aa56467c827d4d2397d9bf6d0f9d799b48736594bda3d93cf7512350a`.


## 2026-10-22 — Run all JC305 deploy areas across three Pico readers

- Step/experiment: `three-reader-jc305-deploy-20261022-053706`
- Change: No firmware or source changes. Ran the repository parallel deploy
  runner across all nine JC305 API areas using three independently enumerated
  physical readers.
- Validation: Elevated host-access dry run found Java reader indices 3–5 with
  stable IDs D92E743D5F640C0F, E0E14525E64AF6ED, and 5CFC5328F5BDE437. The live
  runner completed Security (901 calls), Crypto (466), Service (124), and Core
  (291). Security outcomes: 63 OK, 820 `UNSUPPORTED_PER_SPEC`, 13
  `EXC_RUNTIME`, two `WRONG_RESULT`, one `SW_ERROR`, two skipped by the
  configured exclusion. Crypto: 11 OK, 434 `UNSUPPORTED_PER_SPEC`, 16
  `EXC_RUNTIME`, three `EXC_CARDRUNTIME`, two `SW_ERROR`. Service: 94 OK, 28
  `WRONG_RESULT`, one `SW_ERROR`, one `EXC_RUNTIME`. Core: 271 OK, 15
  `UNSUPPORTED_PER_SPEC`, four optional applet calls unavailable, one
  `WRONG_RESULT`. Cleanup passed for completed CAP tests. Six package install
  attempts (optional Core Ext and Javacardx, JavacardxExt, JavacardxSec,
  JavacardxInt, Biometry) returned `6A88`; each partial package was rolled
  back. Runner exit status was 1 because of these refused INSTALL stages.
- Result: Parallel reader routing and the full deploy-area batch are operational.
  Missing-import refusals match the declared ROM profile. Physical API results
  describe existing board firmware; no current-source firmware was flashed.
- Decision: Treat missing optional imports as profile gaps, not broken rollback.
  Continue with the current-source GlobalPlatform DELETE APDU and lifecycle
  verification identified in `TODO.md`.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261022-053706/`. CSV
  SHA-256 values are recorded in `.agent/HANDOFF.md`.


## 2026-10-22 — Recognize GPPro default ISD selection in PC VPCD

- Step/experiment: `pc-vpcd-default-isd-select-20261022`
- Change: Added recognition for GPPro's case-1 empty-AID SELECT of the default
  ISD, tracked ISD selection separately from applet selection, and added a
  card-manager processing callback seam to the PC VPCD session. This matches
  the existing ARM CCID empty-AID behavior. The host callback remains to be
  wired to SCP03 and card-manager APDUs.
- Validation: `./tools/test-vpcd-session` passed `TEST:SUMMARY pass=21 fail=0`;
  `./tools/build-pc` passed; `./tools/build arm` passed. No RISC-V build,
  physical reader operation, or flash was run. PC build output includes
  dependency Rust warnings and existing packed-structure warnings.
- Result: The 21-case VPCD session test covers the default ISD FCI and routing
  through the callback seam. GPPro originally failed its default SELECT with
  `6999`; full GPPro listing remains unverified until the callback is wired.
- Decision: Continue by implementing card-manager command dispatch in the host
  VPCD path, then exercise the WP6 GPPro lifecycle over Virtual PCD.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/gp-lifecycle-vpcd-20261022/` contains the
  temporary emulator flash copy from the pre-fix reproduction.


## 2026-10-22 — Complete GPPro card-manager lifecycle over PC VPCD

- Step/experiment: `pc-vpcd-gppro-lifecycle-20261022`
- Change: Wired the host VPCD card-manager callback to SCP03, GET STATUS,
  GET DATA, CAP LOAD/INSTALL, and DELETE. On resume, rebuild the supported
  single-instance GET STATUS record from explicit `--applet-aid` and
  `--class-index` arguments so the registry survives emulator process restart.
- Validation: `./tools/build-pc` passed; `./tools/build arm` passed earlier in
  this same implementation step; `./tools/test-vpcd-session` passed 21/21;
  `./tools/test-gp-delete` passed 26/26; `./tools/test-gp-registry` passed
  47/47. With host PC/SC access and isolated scratch flash, GPPro 20.01.23
  `--list`, package `--delete`, CAP `--install`, and `--list` all completed.
  After emulator process restart, GPPro again listed the package and selectable
  applet. The first restart check exposed a missing host-side instance record;
  it passed after reconstructing the record from resume arguments. No Pico was
  accessed or flashed; no RISC-V build was run.
- Result: The former default-ISD `6999` failure is resolved in host VPCD and the
  authenticated package lifecycle passed end to end. The successful restart
  list contains `APP: 4A43416C675465737431 (SELECTABLE)` and
  `PKG: 4A43416C6754657374 (LOADED)`. VPCD still models only one dynamic package
  and one instance; persistent registry enumeration and physical CCID parity
  remain unverified. VPCD presence probing may report `present=false` before a
  GP client connects, so integration harnesses should wait for
  `VPCD:CONNECTED` and then use GPPro itself as the active transport check.
- Decision: Close the GPPro host VPCD lifecycle task. Continue with GET DATA
  coverage, full persistent registry restoration, and multi-package/instance
  behavior without claiming physical-token validation.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/gp-lifecycle-vpcd-20261022/`. GPPro list
  SHA-256 `ea1a46713762c848bf2b6ce9d4753a2ff5c2b24d9986a3a9be19d69f4893bc62`,
  delete `54947d66febaea0c5ae3918d8492528df2bbe991698c44f54da03aaf8a47c5c6`,
  reinstall `66e4a2c3d450cb343ef2815a001fae1502e2bfc1b4f22cd15f64222d5eeca99f`,
  post-restart list
  `f33f82775c0bdbd2c5723a625ed0b2396c13443cf399c0c54b7bb82c8996ad40`.


## 2026-10-22 — Inspect GPPro GET DATA profile on PC VPCD

- Step/experiment: `pc-vpcd-gppro-get-data-info-20261022`
- Change: No implementation change. Ran GPPro 20.01.23 `--info --debug` against
  the current-source VPCD card manager and recorded the requested data tags and
  returned status words.
- Validation: GPPro exited 0. Card Recognition Data (`80CA0066`) returned the
  26-byte `66` TLV and `9000`; it identifies GlobalPlatform and SCP03 i=10.
  CPLC (`80CA9F7F`), IIN (`80CA0042`), CIN (`80CA0045`), Card Capabilities
  (`80CA0067`), and Key Info Template (`80CA00E0`) returned `6A88`, which GPPro
  reported as unavailable without failing. No token was used or flashed.
- Result: Existing GET DATA support matches the current data source: CRD is
  advertised; CPLC and other card-specific data are not fabricated. GPPro does
  not exercise GET STATUS continuation in `--info`; P2=03 and pagination still
  require targeted testing.
- Decision: Keep unavailable card-specific objects unsupported until the card
  has a genuine source for those values. Next test GET STATUS continuation and
  package/module pagination.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/gp-lifecycle-vpcd-20261022/gppro-info.log`,
  SHA-256 `4f3cb951e3269915eb0553fbe060c2998e85f945c3dcd2b856373813623c37a5`.


## 2026-10-22 — Verify authenticated GET STATUS first/next sequences

- Step/experiment: `pc-vpcd-get-status-pagination-20261022`
- Change: No implementation change. Used repeated GPPro `--secure-apdu`
  options so each category's first (`P2=02`) and next (`P2=03`) GET STATUS
  requests shared one SCP03 session.
- Validation: The six authenticated commands for application (`P1=40`), load
  package (`P1=20`), and module (`P1=10`) each returned `9000`. Each first
  request returned the one existing matching entry; each next request returned
  an empty successful response. An isolated next request with no preceding
  first request returned expected `6985`. The log decoder counted all six
  command/response pairs and passed. No token was used or flashed.
- Result: GET STATUS continuation state is verified for the currently supported
  one-entry registry per category. This does not validate pagination among
  multiple packages, applets, or modules.
- Decision: Keep multi-entry pagination open until the PC VPCD registry supports
  multiple records; preserve the validated `P2=02/03` sequencing.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs:
  `artifacts/results/gp-lifecycle-vpcd-20261022/gppro-get-status-pagination.log`,
  SHA-256 `59db893aa6321b40a1ffeb7c272c6c1a503dc5e3e229c33ba66583a860681e81`.


## 2026-10-22 — Recheck GET DATA object encoding

- Step/experiment: `gp-get-data-regression-20261022`
- Change: No source change. Re-ran the focused GET DATA unit suite after the
  official GPPro information query.
- Validation: `./tools/test-gp-get-data` passed all 11 checks, including exact
  Card Recognition Data TLV bytes, `9000` response encoding, and `6A88` for
  unavailable CPLC and unknown objects. No hardware access was involved.
- Result: Unit behavior agrees with GPPro `--info`; no card-specific values are
  synthesized when the emulator has no source for them.
- Decision: Keep `0066` as the advertised GET DATA object; leave CPLC and
  unsupported objects unavailable.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `./tools/test-gp-get-data` console output; no separate log.


## 2026-10-22 — Resume VPCD persistence with allocated package ID

- Step/experiment: `vpcd-persistence-package-id-resume-20261022`
- Change: Added the CAP LOAD-assigned package ID to the PC VPCD
  `VPCD:APPLET_INSTALLED` marker. Updated the transaction probe to accept an
  explicit package ID during restore, and made `tools/test-vpcd-persistence`
  parse and pass the ID selected by CAP LOAD.
- Validation: `./tools/build-pc` passed. Shell/Python syntax checks passed.
  The first full persistence wrapper run failed at applet SELECT (`6F00`):
  resume looked up package 63 although LOAD had allocated package 56. The
  corrected `./tools/test-vpcd-persistence` passed commit/install 3/3, restored
  the actual package ID 56, and passed restore 7/7, including committed field
  `0x33`, static root identity, persistent array child identity, object-field
  alias mutation, and restored array type. This is host VPCD process-restart
  validation only; no physical token was used or flashed. No RISC-V build was
  run.
- Result: Resume tests now follow the dynamic package allocator instead of
  assuming that the input/default package hint is the actual installed slot.
  The GC/persistence fixture passes beyond JCAlgTest. Forced collection under
  pressure while exported/static references are held remains open.
- Decision: Keep dynamic package ID extraction in restart tests; continue with
  GC root survival under allocation pressure and multi-package persistence.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Build log `artifacts/logs/build-pc-vpcd-persistence-20261022T052745Z.log`;
  install log SHA-256 `898e2fb4ad75656b80b5122e882c4e9602d3ba2bbc805ee2d2dd3c60712f39a6`;
  restore log SHA-256 `e1a00ceb6386ca752ef88d806aa1aba5f788bead99a074d578ec4088dd25e9f7`;
  emulator logs and commit transcript are in `artifacts/logs/` under the same
  timestamp.

## 2026-10-22 — Reconcile status and handoff with persistence results

- Step/experiment: `vpcd-persistence-doc-reconciliation-20261022`
- Change: Corrected `STATUS.md` to describe flash-backed lazy restore and the
  passing two-process VPCD regression. Added the package-ID failure, repair,
  evidence, limits, and next GC pressure task at the top of `.agent/HANDOFF.md`.
- Validation: Inspected the existing `tools/test-vpcd-persistence` result and
  matching artifacts; `git diff --check` passed for the documentation and
  progress files. No tests were rerun because this step changes documentation
  only.
- Result: The current high-level status no longer says persistent objects are
  pinned until deletion; the handoff now records that the observed restart is
  process-level VPCD, not a physical power cycle.
- Decision: Next implement focused allocation-pressure coverage for live static
  and exported references; keep multi-package persistence and hardware
  power-cycle validation open.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Existing persistence artifacts under
  `artifacts/logs/` at timestamp `20261022T052745Z`; see preceding entry for
  hashes.

## 2026-10-22 — Stress persistent GC roots and isolate transaction rollback cache

- Step/experiment: `gc-persistent-root-pressure-vpcd-20261022`
- Change: Extended the transaction-probe runner with bounded
  `--gc-pressure-rounds`. Each round allocates 256 short-lived arrays and 256
  objects in separate APDUs, then validates static root identity, persistent
  array child identity, object-field aliases/mutation, and restored array type.
  Increased the existing allocation batches from 64 to 256 objects.
- Validation: Probe CAP compilation passed. Python syntax and targeted
  `git diff --check` passed. Current-source VPCD focused run with cases 14–17
  and 8 stress rounds passed `TEST:SUMMARY pass=54 fail=0 elapsed_s=9.822`.
  No physical device was used. The first broader run stopped at the existing
  case-5 rollback check; rerunning case 5 alone reproduced response `22 9000`
  instead of expected `11`. Source inspection established that rollback
  restores flash snapshots but not the live `JC_Instance` field cache. No fix
  to transaction handling was made in this step.
- Result: Repeated boundary collection preserves the exercised static and
  persistent object graph under 256-array/256-instance batches. Exported RMI
  roots and direct flash recovery after RAM reclamation remain open. The abort
  cache coherence issue is a separate concrete JCSystem defect requiring the
  next implementation step.
- Decision: Fix persistent field cache invalidation/reload on abort, then run
  cases 5 and 1–7 plus native transaction tests. Keep GC root stress in the
  regression suite and expand it to exported references later.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Passing transcript
  `artifacts/logs/gc-pressure-persistent-roots-256-vpcd-transcript-20261022.log`
  SHA-256 `2b8859e49d463c8a46244a5b74923f37fa092621258cce612f8c1ab6cb5acfff`;
  standalone rollback reproduction transcript
  `artifacts/logs/jcsystem-transaction-case5-vpcd-transcript-20261022.log`
  SHA-256 `f48001859004c6e65fbb3ea24c32f60dc3c7b536713bc7f878f8cffe2825cd70`.
  Initial 64-item full-run failure is retained at
  `artifacts/logs/gc-pressure-persistent-roots-vpcd-transcript-20261022.log`.

## 2026-10-22 — Reload persistent fields after transaction abort

- Step/experiment: `jcsystem-abort-cache-reload-vpcd-20261022`
- Change: Added a persistent-write rollback generation to
  `FlashMemory_Handler`. `JC_Instance::getFields()` detects a generation change
  and reloads the instance's fields from the rolled-back records. Updated the
  VPCD probe runner to reselect the applet after its intentional unknown-AID
  deselect case, allowing later cases to run.
- Validation: `./tools/build-pc`, `./tools/build arm`, and
  `./tools/build riscv` passed. Ordered JCSystem transaction cases 1–7 passed
  10/10, with abort returning `0x11` and subsequent commit retaining `0x33`.
  The full current-source VPCD suite passed 80/80 in 19.322 s. Native
  `./tools/test-persistent-transaction` passed with `TEST:SUMMARY fail=0`.
  The initial full-suite rerun passed transactions but stopped at case 75
  because the harness had not reselected after case 74; after fixing the
  harness sequencing, the full suite passed. No physical token was used or
  flashed.
- Result: Persistent field reads after abort now agree with restored flash
  snapshots. The cache is preserved during normal access and invalidated only
  after rollback. Hardware transaction behavior remains unverified.
- Decision: Keep rollback-generation invalidation and the case-74 reselection
  in regression coverage. Remaining transaction work is commit-capacity
  reporting and physical power-cycle verification; remaining GC work is
  pressure with exported RMI references.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Full VPCD transcript
  `artifacts/logs/jcsystem-transaction-full-vpcd-fixed-runner-transcript-20261022.log`
  SHA-256 `d10c03ff46c4951deb40ccbe19d470178626807743b571dea809469b7e4de6d9`;
  ordered cases transcript
  `artifacts/logs/jcsystem-transaction-cases1-7-vpcd-transcript-20261022.log`
  SHA-256 `48b82fbf0e389b3706329cc3ccd2d5125617b62404ee1f4a74ed53e7c321e961`;
  GC pressure log/transcript are listed in the preceding entry.

## 2026-10-22 — Revalidate RMI array and interface paths; classify Security mismatches

- Step/experiment: `rmi-arrays-revalidate-and-security-triage-20261022`
- Change: No runtime source changes. Re-ran the current-source RMI array and
  remote-interface VPCD fixture. Inspected the Security CAP catalog and the
  matching current-source results to classify its two downstream mismatches.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-arrays-vpcd` through
  the elevated host-access route passed `./tools/build arm`, `./tools/build-pc`,
  fixture conversion, the primitive-array/protocol VPCD suite (`22/22`), and
  interface-reference SELECT (`3/3`). `git -c core.whitespace=cr-at-eol diff
  --check` passed. No physical device was accessed, no RISC-V build ran, and
  no firmware was flashed.
- Result: RMI byte/short/boolean array paths and class/interface reference
  descriptors remain green. `int[]` is still unverified because the JCDK
  converter omits its remote methods from fixture metadata. Security's two
  decoded `Key.isInitialized()` mismatches are dependent on transient AES key
  setup calls that are themselves `UNSUPPORTED_PER_SPEC`; the SDK KeyBuilder
  currently supports persistent AES/DES only. This is unavailable-feature
  fallout, not a reproduced VM defect.
- Decision: Keep `int[]` metadata as the next RMI test gap. Do not implement
  transient key types or crypto algorithms only to improve the Security
  support count; test key initialization with a supported key if investigating
  the VM semantics.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: RMI INVOKE transcript
  `artifacts/logs/rmi-array-invoke-vpcd-20261022T055319Z.log` SHA-256
  `48f3ab5334d911d7b5d8b0b7d2c22974219448b19ad08e5a6d11c11a83c304f3`;
  interface transcript `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T055319Z.log`
  SHA-256 `5cd94d7b10efcc5a4826b9d3088d0ee83c3acc941fdf9060f52171228038eb7e`;
  ARM build SHA-256 `3cbbb83864ccdea8ae0833a33faf05d02e4d9307ef349c3961dbf3eefefea956`;
  PC build SHA-256 `eff21e439c279a3600258a13393b84cd2a613e7c649aba182e2f79c999b29ccd`.

## 2026-10-22 — Recheck official JCRMI client interoperability

- Step/experiment: `jcrmi-client-vpcd-recheck-20261022`
- Change: No runtime source changes. Re-ran the bundled Oracle Java Card 2.2.1
  JCRMI proxy against the current-source VPCD fixture.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-client-vpcd` through
  the elevated host-access route passed the PC build and fixture conversion.
  The proxy selected with its unmodified `Le=7F`, invoked `Counter.add(short)`
  and same-context SensitiveResult checks, and decoded runtime and checked
  exception responses. No physical reader was selected; no RISC-V build or
  flash was run. `git -c core.whitespace=cr-at-eol diff --check` passed.
- Result: Client responses matched the previously validated fixture: add
  returned 3, SensitiveResult returned true, runtime subclass decoding exposed
  `RuntimeException`, and the checked subclass surfaced as
  `UndeclaredThrowableException` because the protocol identifies the nearest
  exported parent type. This is the documented proxy behavior; broader custom
  exception hierarchy coverage remains open.
- Decision: Keep the JCRMI client path in the regression set. Continue with
  wider custom hierarchy/API reason tests and the `int[]` CAP metadata gap.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Emulator transcript
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261022T055543Z.log`
  SHA-256 `b40963892744d18ba78e4d574082e13f0d75a04b1d6fd661b6892e481a6618a4`;
  PC build log `artifacts/logs/build-pc-rmi-client-20261022T055543Z.log` SHA-256
  `46f76dbff7d0b7002bae5add25f4ef3a0f09b0c476aa1260a01bfd583b09cdd6`.

## 2026-10-22 — Verify exported RMI roots survive collection under pressure

- Step/experiment: `rmi-exported-root-gc-pressure-vpcd-20261022`
- Change: Added `Counter.allocationPressure()` to the CAP 2.2 fixture; it
  allocates 256 short-lived 64-byte arrays. Updated the VPCD regression to
  return a child object, run this pressure APDU with host GC enabled, then
  invoke the child.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-arrays-vpcd` through
  elevated host access passed `./tools/build arm`, `./tools/build-pc`, fixture
  conversion, 23/23 class-form VPCD cases, and 3/3 interface-form checks. The
  emulator trace reports `arrays=5/261 reclaimed_arrays=256 ... complete=1`;
  the next child invocation returned `8100009000`. `git -c
  core.whitespace=cr-at-eol diff --check` passed. No physical device, RISC-V
  build, or flash was used.
- Result: The active RMI registry keeps the returned child reachable through a
  real APDU-boundary collection that reclaimed all 256 temporary arrays. The
  first test attempt had an incorrect extra length byte in its expected short
  encoding; corrected expectation `81FF809000` passed. Persistent flash
  recovery after RAM reclamation remains open.
- Decision: Keep the regression APDU between child export and child invocation.
  Next exercise persistent-record restoration under allocation pressure, then
  return to the outstanding `int[]` converter metadata gap.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Main transcript
  `artifacts/logs/rmi-array-invoke-vpcd-20261022T060244Z.log` SHA-256
  `a232c6691824f8554aa4ced99bb64f490cc53340d22299386b743a9e74ffd2d7`;
  interface transcript `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T060244Z.log`
  SHA-256 `7911a43152682d8ff536625426d56a93d92173f67a5e56369883c15864dc734b`;
  ARM build SHA-256 `58bc9f58d84ae14884ac54d6abacdff0c689366f7219fe1cd4927ef4879fe6bb`;
  PC build SHA-256 `b105af212a363cda56584aaa0f5aed9f8c1bdc78837069405601ca98b2c6f37f`.

## 2026-10-22 — Stress restored persistent aliases with collection enabled

- Step/experiment: `vpcd-persistence-gc-pressure-after-restore-20261022`
- Change: Extended `tools/test-vpcd-persistence` with configurable
  `PICO2_VPCD_PERSISTENCE_GC_ROUNDS` (default 4, maximum 1000). The fresh
  process restore now enables `PICO2_HEAP_GC=1` and passes the configured
  pressure rounds to the JCSystem probe.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed
  PC build, probe CAP build, commit/install 3/3, and fresh-process restore plus
  stress 31/31. Four rounds each allocated 256 arrays and 256 objects, then
  checked static-root identity, persistent array child, persistent object
  alias mutation, and restored array type. Emulator traces show complete walks
  reclaiming 256 arrays and 256 instances in each round. `git -c
  core.whitespace=cr-at-eol diff --check` passed. No physical device, RISC-V
  build, or flash was used.
- Result: A committed applet restored in a new emulator process retains its
  persistent reference graph across repeated collection of temporary arrays
  and objects. The persistent graph stays rooted, so this does not yet validate
  reloading a persistent object after its RAM instance is itself reclaimed.
  PC build emitted existing Rust, Java source-level, and packed-member
  warnings; ROM-mask build completed despite unavailable remote Maven metadata.
- Decision: Preserve the test as process-restart GC regression. Next inspect
  the root/reload model to determine whether safely reclaiming an unrooted
  flash-backed object and recreating it on demand is supported by current APIs.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Restore client transcript
  `artifacts/logs/vpcd-persistence-restore-20261022T060544Z.log` SHA-256
  `14027e7b2c20292ee6d51ade1d050bd6e422c1ee55290d483714ee4c711f3759`;
  restore emulator trace `artifacts/logs/vpcd-persistence-restore-emulator-20261022T060544Z.log`
  SHA-256 `a10a3ec7132d3aa58b0950b4ff6722d1f137a866ab3e22f34e259d28a88748c6`;
  install client transcript SHA-256 `b3fe0fd1c91b154f0ca6a65361c28b79d5608082f81c60cda283700e40f7d076`;
  install emulator trace SHA-256 `48be4e8a84f79b8e6082ad523d5e2d30bf94e08b2f0ab313a29101efb4ec2d88`;
  PC build SHA-256 `5ea37dfd57c8f2c0780e291945a24a24e5edcd0275275788d86003dba38f7817`.

## 2026-10-22 — Keep persistent array aliases valid through collection

- Step/experiment: `persistent-array-stable-identity-after-gc`
- Change: Moved persistent array bodies to a stable tag derived from their first
  field tag plus reserved element index `0xffff`; each static or instance
  reference field now stores an alias to the body. Added probe cases 80/81 to
  read and clear one of two static aliases, then read the remaining alias after
  GC. Nested instance-array serialization now follows the same identity path.
- Validation: `./tools/build arm` passed. Elevated
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed: install
  `TEST:SUMMARY pass=3 fail=0`; fresh-process restore and four GC pressure
  rounds `TEST:SUMMARY pass=33 fail=0`. The trace after clearing alias A
  recorded `arrays=7/8 reclaimed_arrays=1`; the next operation restored B from
  flash and returned `019000`. `git -c core.whitespace=cr-at-eol diff --check`
  passed. No physical board, flash, or RISC-V build was used.
- Result: The prior failure (`case 81: expected 01, got 00`) was caused by the
  first static reference field doubling as the array's storage tag. Clearing
  that field overwrote the record targeted by other aliases. Stable array
  identity fixes current-format VPCD persistence. Migration of already stored
  legacy field-owned array records and physical power-cycle behavior remain
  unverified. PC build emitted existing Rust and packed-member warnings.
- Decision: Keep the lazy restoration regression enabled. Next cover alias
  reclamation with arrays nested in persistent object graphs, then return to
  the RMI `int[]` converter metadata gap.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Restore transcript
  `artifacts/logs/vpcd-persistence-restore-20261022T061524Z.log` SHA-256
  `7d8ce80bb26891618c8a2ac96330aecdff213605e55e9148000928bb2c6cadc5`;
  emulator trace
  `artifacts/logs/vpcd-persistence-restore-emulator-20261022T061524Z.log` SHA-256
  `6705cc897a446bc6cf37807a5cb43e84fdcc02d65791cfac6e160078469dc445`;
  install transcript
  `artifacts/logs/vpcd-persistence-commit-20261022T061524Z.log` SHA-256
  `aae73c89fc7ad4f603fcfd41fbbb1ce2a1b67b1189fa08f39912906a61ca5af2`.

## 2026-10-22 — Revalidate deploy JavacardxSec on current VPCD

- Step/experiment: `deploy-javacardxsec-vpcd-after-persistence-change`
- Change: Re-ran the official deploy API 3.0.5 JavacardxSec client against a
  freshly built PC VPCD image after the stable persistent-array identity fix.
- Validation: Elevated `PICO2_HOST_USB_ACCESS=1
  ./tools/test-javacardxsec-vpcd` passed current PC build, CAP install, applet
  selection, and all 84 deploy API calls (`OK=84`). This is current-source
  VPCD validation only; no physical device was accessed and no RISC-V build
  was run. `git -c core.whitespace=cr-at-eol diff --check` passed.
- Result: The persistence change did not regress the JavacardxSec deploy
  profile. PC build continues to emit existing Rust and packed-member warnings.
- Decision: Continue official deploy validation with the current-source Core
  applet, including the rows that require separately installed Multi/Ext
  scenario packages.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: `artifacts/results/javacardxsec-vpcd-20261022T061927Z/`;
  CSV SHA-256 `244b42beadbe2594e461a4410fc1656c60f040f9e62930a00cab7e54e04b2566`;
  emulator SHA-256
  `0edf8bc475a511a6bb7073359573da4f85dfdd62b89f436494f644454e78621f`.

## 2026-10-22 — Isolate current Core deploy failure from stable array identity

- Step/experiment: `core-deploy-vpcd-array-serialization-ab`
- Change: Temporarily reverted only the stable persistent-array body-tag
  serialization paths for an A/B run, then restored the saved stable source.
  No implementation change was retained from the experiment.
- Validation: The pre-change PC build passed. Elevated VPCD Core deploy run
  completed 291 calls. Stable and reverted runs match on every call's status,
  class, method, and argument: `OK=208, EXC_RUNTIME=55,
  UNSUPPORTED_PER_SPEC=11, WRONG_RESULT=8, APPLET_NOT_INSTALLED=8, SW_ERROR=1`.
  `./tools/build arm` passed after restoring stable source. No RISC-V build,
  physical USB access, or flash was performed.
- Result: The stable body-tag change alone is not the cause of this Core
  failure. The first exceptions occur in the `Util.arrayCopy` group; the APDU
  trace later ends with `6F00` / `SecurityException`. Root cause remains
  unresolved. The 8 unavailable applet rows require scenario packages outside
  this single-package VPCD run.
- Decision: Keep the stable array identity change and trace the first Util
  failure into native array access before making another storage change.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Baseline CSV SHA-256
  `8f3e25e15082cada0ae7d1f1be0232d272e81d41a2ce8c5d2e28d6eae2e42c39`;
  stable CSV SHA-256
  `d81221ac923575193414cdba4b3aaf523ccad67c4fd021a29c7255440831cd02`;
  ARM build log SHA-256
  `d0e8935fe5c86d7f1b1f980105120a87f6eaa7682ab9ebf53f5e3df2a0a5deb0`;
  artifacts in `artifacts/results/deploy-current-core-vpcd-20261022/`.

## 2026-10-22 — Preserve caches after empty transaction aborts

- Step/experiment: `empty-transaction-abort-cache-generation`
- Change: Advanced the persistent rollback generation only when the rollback
  snapshot list contains records. Aborting a transaction that made no
  persistent writes no longer forces all live persistent instance fields to
  reload. The stable persistent-array identity implementation remains intact.
- Validation: `./tools/build arm` and `CHOUPI_JCVM_DEBUG=OFF
  ./tools/build-pc` passed. Elevated current-source API 3.0.5 Core VPCD run
  completed 291 calls: `OK=272, UNSUPPORTED_PER_SPEC=11,
  APPLET_NOT_INSTALLED=8`; zero `EXC_RUNTIME`, `WRONG_RESULT`, and `SW_ERROR`.
  Elevated `./tools/test-jcsystem-transaction-applet --case 1 --case 5
  --case 6 --case 7` passed 7/7, including abort-after-write restoring field
  `0x11`, readback after abort, commit to `0x33`, and readback after commit.
  The first local-socket attempt was rejected by the restricted process
  context (`Operation not permitted`); rerunning the same bounded command via
  the elevated host-access path passed. RISC-V and physical hardware were not
  used; nothing was flashed. `git -c core.whitespace=cr-at-eol diff --check`
  passed.
- Result: The earlier 55 runtime errors and 8 wrong results were caused by an
  empty abort spuriously invalidating runtime caches, not by stable array
  serialization. The remaining eight unavailable deploy rows require the
  separate Multi/Ext scenario packages; 11 unsupported results are expected
  per the implementation profile.
- Decision: Keep the generation guard and the full Core deploy VPCD suite in
  regression coverage. Continue with the next priority in `TODO.md`.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Core CSV SHA-256
  `fea66263ad356885aa66ba5bcf0f2987159ef6c61464ae80aeca0d4af5860dab`;
  Core emulator SHA-256
  `36bf328eaa45c237127f86c437a1672afab50393e5c928861400599a83aa3db1`;
  transaction transcript SHA-256
  `f6496f9053afd003bc9ad00377de51c5f2eae410de4eeda03b96bb17cdb83747`;
  ARM build log SHA-256
  `b012017405a6e40b723efe88da895ce194cfde18bf076cc1f7251af72dd7540b`;
  PC build log SHA-256
  `5d38a5399a298bf7e30daafd47e410b0cc4ef8a71d084dd2c1d2781f4f43e3b0`;
  files are under `artifacts/results/deploy-current-core-vpcd-20261022/` and
  `artifacts/logs/`.

## 2026-10-22 — Cover empty abort between transient-array lifecycle checks

- Step/experiment: `empty-abort-transient-instance-regression`
- Change: Added transaction probe case 79. It begins and aborts an empty
  transaction, then checks that the live `CLEAR_ON_RESET` and
  `CLEAR_ON_DESELECT` instance fields retain their references, values, and
  event types. The case runs between transient-array setup and the existing
  deselect-lifecycle check.
- Validation: Elevated `./tools/test-jcsystem-transaction-applet --case 30
  --case 79 --case 31` built the probe CAP and passed 5/5 assertions. Case 79
  returned `03`, and the following deselect case returned `03`. No physical
  reader, RISC-V build, or flash was used. `git -c
  core.whitespace=cr-at-eol diff --check` passed.
- Result: The no-op abort regression is now directly covered without changing
  the transaction's persistent state; transient fields remain available for
  their proper reset/deselect lifecycle.
- Decision: Keep case 79 in the focused probe and continue with the next open
  implementation priority.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Transcript
  `artifacts/logs/jcsystem-empty-abort-transient-root-20261022.log` SHA-256
  `f7a33d80d7a8901413129fde4bd50287e678eeb290163c902501b6723927b2dc`.

## 2026-10-22 — Recheck persistence and GC after rollback-generation fix

- Step/experiment: `vpcd-persistence-after-empty-abort-fix`
- Change: No additional runtime changes. Re-ran the full persistent CAP
  install/restart/alias regression after guarding rollback-generation changes
  on a nonempty snapshot list.
- Validation: Elevated `PICO2_HOST_USB_ACCESS=1
  ./tools/test-vpcd-persistence` passed the PC build, install/commit (3/3),
  fresh-process restore, stable static array aliases, and four GC pressure
  rounds (33/33). Each pressure round allocated 256 arrays and 256 objects;
  static roots, persistent array/object aliases, and restored array type passed
  after every round. `git -c core.whitespace=cr-at-eol diff --check` passed.
  No physical reader, RISC-V build, or flash was used.
- Result: The no-op abort fix preserves the previously passing persistent-array
  restoration and collection behavior.
- Decision: Keep the implementation and all three regression layers: official
  Core deploy, focused transaction/transient probe, and process-restart GC
  persistence test.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Install transcript
  `artifacts/logs/vpcd-persistence-commit-20261022T073040Z.log` SHA-256
  `5761465b67d6459bf2709cb71544190cb5bf689b7310c645fe574bfe3c7ff0f9`;
  restore transcript
  `artifacts/logs/vpcd-persistence-restore-20261022T073040Z.log` SHA-256
  `528027aef95798bb47e2a806dd57da16f056aaaba1e9b5c5d79d2c00b71eefe8`.

## 2026-10-22 — Refresh caches after failed snapshot commits

- Step/experiment: `failed-commit-snapshot-cache-generation`
- Change: When `commitTransaction()` gets an `fs_sync` error and rolls back a
  nonempty snapshot list, it now advances the same rollback generation used by
  explicit abort. Transactions with no persistent writes continue to preserve
  live caches.
- Validation: `./tools/test-persistent-transaction` passed all checks,
  including injected snapshot-commit failure, rollback values, and transaction
  closure. Final `./tools/build arm` and `CHOUPI_JCVM_DEBUG=OFF
  ./tools/build-pc` passed. `git -c core.whitespace=cr-at-eol diff --check`
  passed. No RISC-V build, physical reader, or flash was used.
- Result: Both explicit abort and failed-commit rollback invalidate caches
  exactly when persistent snapshots were restored; empty rollback leaves the
  generation unchanged.
- Decision: Keep one shared generation rule for both rollback paths. Earlier
  evidence for the official Core VPCD run, empty-abort transient regression,
  and persistent array GC suite remains valid because this change affects only
  the injected `fs_sync` failure path.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Native transaction log SHA-256
  `5bff5a783e34b05cb3a21e9adb5608dcd2401af45d54ca5785829d0fcc519c33`;
  ARM build log SHA-256
  `a5c2676996d8b93a9b58c40d7dbd0a4d0a8730d613270ef276cac721baa45857`;
  PC build log SHA-256
  `16d2e1296a6fc93e39e7436f34cbc8e553e5cfa6abce7e8dc5eacda533e05cdf`.

## 2026-10-22 — Cover a nested persistent reference graph

- Step/experiment: `nested-persistent-array-graph-gc`
- Change: Added transaction-probe case 82. A persistent holder contains an
  `Object[]` with two references to one byte array; the case checks restored
  identity, shared mutation visibility, and original value. Added it to the
  process-restart persistence suite and each GC-pressure round. Updated
  `TODO.md`, `STATUS.md`, `.agent/HANDOFF.md`, and `.agent/QKB.md` with results
  and fixture boundaries.
- Validation: `./tools/build arm` and `./tools/build-pc` passed;
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed install 3/3
  and restore/stress 38/38, including four rounds of 256 temporary arrays and
  256 temporary instances. The new graph check passed after restart and in all
  four GC rounds. `./tools/test-vpcd-session` passed 21/21;
  `./tools/test-gp-delete` 26/26; `./tools/test-gp-registry` 47/47; and
  `./tools/test-gp-get-data` 11/11. `git -c core.whitespace=cr-at-eol diff
  --check` passed. Physical reader preflight found no Pico PC/SC readers; no
  hardware run, RISC-V build, or flash was performed.
- Result: Persistent object → reference array → aliased byte-array identity is
  verified after process restart and repeated collection. JCDK 3.0.5 rejects
  `Object[][]` during CAP conversion. A direct `Object[]`-in-`Object[]` attempt
  installed with `SecurityException` at `AASTORE`; that failure remains
  unisolated and is recorded in `.agent/QKB.md`.
- Decision: Keep the passing holder graph in regression coverage. Investigate
  nested `AASTORE` firewall ownership separately before claiming array-of-array
  writes work.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Restore log SHA-256
  `5e3861913c0a48f5c70f63d23b646fa9aeee93bea465f56f312cfeab76f22be0`;
  emulator restore log SHA-256
  `73b535a2af59df819f1c0cf54ef13228756d2167931a08879143303063cdd744`;
  install log SHA-256
  `d9a3b7c2c44cfd7f26081f5fcf638510044951a38e6c9f345fcdf7091ad58b2e`;
  emulator install log SHA-256
  `581312a05639d0954aad9f62949c98c0f4236fb2d5aaaaa1fcf1ad1768017f14`;
  failed initial nested-array emulator log SHA-256
  `c08e959b0821c5552db2312dd4f153b0f648d6b45acdb40a95be53b698c87100`.

## 2026-10-22 — Validate fresh GPPro card-manager lifecycle over VPCD

- Step/experiment: `pc-vpcd-gppro-fresh-card`
- Change: Made `--class-index` optional for PC VPCD starts so the host emulator
  can represent an empty card manager. Per-package CAP data is now exposed as
  loaded only after package storage commits; explicit class-index mode remains
  available for resume tests.
- Validation: `./tools/build-pc` passed. `./tools/test-vpcd-session` passed
  21/21 earlier in this continuation; this change only touches `main_pc.cpp`.
  After activating the local reader with `pcsc_scan -r`, GPPro 20.01.23 passed
  initial ISD-only `--list`, SCP03, JC305 CAP `--install`, listing the
  selectable applet and loaded package, applet DELETE, verification that the
  package remained, package DELETE, and final ISD-only listing. No physical
  PC/SC reader was used; all three Picos were in BOOTSEL. No ARM/RISC-V build,
  flash, or hardware test was done.
- Result: The host GPPro path now completes the single-package lifecycle from
  a reproducible empty-card state.
- Decision: Next verify SELECT, GETVERSION, and JCSystem calls after external
  GPPro installation, then verify the saved image after a clean emulator
  restart. Multi-package VPCD registry support remains open.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: GPPro transcript SHA-256
  `5ad24ff141b3fba7c92b6b6ae0847d468d7a53af81c00f44e59f538775a92679`;
  emulator log SHA-256
  `d3095f1ec86375a6cfe7e3722984e6979cf1d117b61a46791b69855f175c7571`;
  PC/SC activation log SHA-256
  `fb0167c87354f218d690dbbb16b5276f5e507f60910a3284af5d4f72f31906de`.

## 2026-10-22 — Invoke JCAlgTest after GPPro CAP installation

- Step/experiment: `pc-vpcd-gppro-installed-applet-invoke`
- Change: Extended `tools/JCAlgTestApduProbe.java` to accept an exact `--reader`
  name while retaining its default physical Pico selection. This lets the same
  SmartcardIO probe exercise GP-installed applets through Virtual PCD.
- Validation: GPPro 20.01.23 installed the supplied JC305 CAP on a fresh host
  VPCD manager. `PICO2_HOST_USB_ACCESS=1 ./tools/probe-jc-algtest --reader
  'Virtual PCD 00 00' --version-only` then passed SELECT, GETVERSION
  (`1.8.2_jc305`), and `JCSystem` info, each with `9000`. The probe wrapper
  compiled Java through `tools/env`. No physical reader was used; no ARM/RISC-V
  build or flash was run.
- Result: The card-manager LOAD/INSTALL path produces an applet that executes
  Java Card entry points on the host VM, including JCSystem info.
- Decision: Next validate cleanly saved persistent state and restore across a
  fresh emulator process. The PC registry still models one dynamic package and
  instance; no three-device test is possible while all boards are in BOOTSEL.
- Commit: Not created; `.git` metadata is read-only in this environment.
- Artifacts/logs: Probe log SHA-256
  `43ad960c12553f23c7e8edf971bf27ff4a093649b7ad0c60e7ff55705258e180`;
  GPPro install log SHA-256
  `75f7268818bfc5fe84290c7574b6d40e7f430a4d919b24c354b8848007202319`;
  emulator log SHA-256
  `e986c8851223fb021862ac1fdca03ae379e2eefb05310512dafd5e472bfca28e`.

## 2026-10-22 — Investigate host VPCD clean-save timeout

- Step/experiment: `pc-vpcd-idle-save-timeout`
- Change: Removed the unverified `poll()`-based timeout experiment after it
  failed to wake the CHOUPI PC emulator's VPCD socket loop. Kept the existing
  socket receive-timeout implementation unchanged; no timeout behavior is
  considered viable yet.
- Validation: `./tools/test-vpcd-protocol` passed 19/19 and `./tools/build-pc`
  passed. GPPro installed `AlgTest_v1.8.2_jc305.cap` on `Virtual PCD 00 00`, but
  the emulator did not exit after the idle deadline. An isolated 2-second idle
  deadline was bounded at 12 seconds and exited only when external `timeout`
  killed it (status 124); the image stayed 32,768 bytes. `git diff --check`
  passed. No ARM/RISC-V build, flash, or physical test was run.
- Result: The GPPro install path works, but a clean emulator save/restart could
  not be validated. External termination is not a substitute for a clean save.
- Decision: Investigate host emulator syscall/socket readiness and implement a
  graceful VPCD disconnect/wakeup before retrying persistence across processes.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: Isolation log SHA-256
  `f2e400a0a61e00322b0d32bfebd2ae02836985a5514d72d44a26df7e142e9f35`;
  GPPro output and emulator log are under
  `artifacts/results/gp-vpcd-restart-20261022/`.

## 2026-10-22 — Validate VPCD idle save and native-install restart

- Step/experiment: `pc-vpcd-idle-save-and-restart`
- Change: Wired the remaining APDU-idle duration from `run_vpcd_transport()` to
  the framed VPCD reader's `poll()` timeout. Track idle from the last APDU so
  Virtual PCD ATR/power control frames cannot keep an otherwise idle process
  alive. The configured deadline now reaches the blocking read.
- Validation: `./tools/test-vpcd-protocol` passed 18/18 and `./tools/build-pc`
  passed. With Virtual PCD active, a 2-second idle run emitted
  `VPCD:IDLE_TIMEOUT seconds=2`, `VPCD:DISCONNECTED result=0`, exited 0, and
  saved 524,288 bytes from a 32,768-byte initial image. Native `--install` of
  JCAlgTest package 56/class 148 then passed GPPro listing and the SmartcardIO
  SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem probe before shutdown and
  again after restarting from the saved image. The second process also exited
  cleanly on its 12-second idle deadline. No ARM/RISC-V build, physical board
  operation, or flash was run. `git diff --check` passed.
- Result: Clean host VPCD process-restart persistence and post-restart applet
  execution pass for the native install path. The external GPPro LOAD/INSTALL
  variant remains unresolved: its fresh-process restore fails at package 63,
  class 148 (`RESTORE_READ_FAIL`, tag `03003F94FF`), although GPPro GET STATUS
  reports the applet. This limitation is in `.agent/QKB.md`.
- Decision: Use the native install restart test as a regression. Next isolate
  persistent instance state from the external GPPro LOAD/INSTALL path.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: Native install logs and image under
  `artifacts/results/gp-vpcd-cli-install-restart-20261022/`; install emulator
  SHA-256 `5e62d7eb05917c4cbac8c9eb48136ba24bed5fa39097c70f144cdcbf2f77bebb`;
  restore emulator SHA-256
  `67f55f47920b6be475a4c9371a28bbd196399522c1fd61feb0ff49f16c0ec123`;
  restored image SHA-256
  `2b3fa5fecc122dcadd9c024f91be124cd906780135ec6d65e058374219bb2ba2`.

## 2026-10-22 — Correct VPCD external-install restart metadata

- Step/experiment: `pc-vpcd-gppro-restart-package-id`
- Change: Corrected the restart fixture to use the package ID allocated by CAP
  LOAD (56 for the first dynamic package) instead of the preconfigured fallback
  ID 63. Updated STATUS, TODO, QKB, and HANDOFF to distinguish working applet
  persistence from the remaining automatic-registry discovery limitation.
- Validation: On the GPPro-installed saved image, resumed CHOUPI with
  `--package-id 56 --class-index 148`; `RESTORE_READ_OK` returned for tag
  `03003894FF`, and JCAlgTest SELECT, GETVERSION (`1.8.2_jc305`), and JCSystem
  info passed with `9000`. The emulator then exited on its 15-second idle
  deadline with status 0. The earlier ID 63 attempt failed at tag
  `03003F94FF`; that mismatch explains the false failure. No ARM/RISC-V build,
  physical-device operation, or flash was run. `git diff --check` passed.
- Result: GPPro CAP LOAD/INSTALL and applet state both survive a clean VPCD
  emulator restart when the resumed process uses the actually allocated package
  ID. Multi-package registry discovery is still missing.
- Decision: Add or expose allocated package metadata for restart automation,
  then extend PC VPCD beyond its single-package model. Physical validation stays
  separate.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: Restore log SHA-256
  `4cd10afbad3077bedcada5a0ecd57ce2435688c8e4e67313fb837fbfaf0da02e`;
  probe log SHA-256
  `43ad960c12553f23c7e8edf971bf27ff4a093649b7ad0c60e7ff55705258e180`;
  saved image in `artifacts/results/gp-vpcd-restart-final-20261022/FLASH_MEMORY`.

## 2026-10-22 — Run card-manager regressions after restart fix

- Step/experiment: `pc-vpcd-card-manager-regression`
- Change: No code change; ran focused regressions after wiring the VPCD idle
  deadline and correcting external-install resume metadata.
- Validation: `./tools/test-vpcd-session` passed 21/21;
  `./tools/test-gp-delete` passed 26/26;
  `./tools/test-gp-registry` passed 47/47;
  `./tools/test-gp-get-data` passed 11/11; `git diff --check` passed.
  Physical hardware remained untouched; no firmware build or flash was run.
- Result: Existing session and GP card-manager behavior remains green.
- Decision: Continue with automatic host VPCD package/class registry discovery;
  physical validation is separate.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: Test summaries are in command output; no extra logs.

## 2026-10-22 — Finalize APDU idle deadline accounting

- Step/experiment: `pc-vpcd-idle-deadline-after-response`
- Change: Reset the idle clock after an APDU response is successfully written,
  so an APDU that takes a long time to execute does not consume the following
  idle interval before the card has even answered.
- Validation: Elevated `./tools/test-vpcd-protocol` passed 18/18;
  `./tools/build-pc` passed; `./tools/test-vpcd-session` passed 21/21. A final
  bounded VPCD idle run exited 0, emitted `VPCD:IDLE_TIMEOUT seconds=2` and
  `VPCD:DISCONNECTED result=0`, and saved exactly 524,288 bytes. The full
  GPPro install/restart and applet probe passed in the preceding step. No
  physical board, flash, ARM build, or RISC-V build was used.
- Result: The clean-save deadline is both wired to blocking reads and measured
  between completed APDU exchanges.
- Decision: Continue with automatic package/class metadata discovery for PC
  VPCD resume. Do not flash while boards are in BOOTSEL and ARM-only builds are
  not paired with the other required architecture build.
- Commit: Not created; `.git` metadata is read-only.
- Artifacts/logs: `artifacts/logs/vpcd-idle-timeout-final-20261022.log`, SHA-256
  `acbe428d3679fd17439bd2bdc5139deaf9ef80877f0b866288297319600678fa`.

## 2026-10-22 — Partition existing worktree into commits

- Step/experiment: `review-and-commit-dirty-worktree`
- Change: Grouped existing work by purpose: CHOUPI dependency sources, JCVM
  runtime source, regression fixtures, developer/deploy scripts, captured data,
  and project status/handoff documentation. Added root `.gitmodules` entries
  so the three committed CHOUPI dependency gitlinks have usable clone URLs.
- Validation: Reviewed tracked/untracked status, staged file lists, and commit
  summaries. `git diff --check` on the full pre-commit tree surfaced whitespace
  reports caused by CR characters in Windows-style source/log lines; those
  files were preserved unchanged. No builds or tests were run because this step
  only organized already existing changes.
- Result: The source, tests, scripts, and tracked data groups are committed
  independently. Ignored CAPs, toolchains, build artifacts, and generated
  result files were not staged. Generated files and uninitialized nested
  optional repositories inside the Java Card SDK submodules remain local.
- Decision: Keep any remaining generated or optional nested submodule content
  out of source commits; review current `git status` before future staging.
- Commit: Dependency source commits `83323a4`, `2fedf58`, `af6817f`; root
  dependency registration `47c8d35`; runtime `6ee8ab9`; tests `d3f1ac3`; tools
  `fc14b73`; data `507ab39`; documentation `9c2d41d`.
- Artifacts/logs: Existing tracked logs and metrics are in `507ab39`; ignored
  generated artifacts were left untouched.

## 2026-10-22 — Scope JC305 interface remaps and validate AES/SHA static calls

- Step/experiment: `jc305-interface-remap-scope-physical-performance`
- Change: `bc_invokeinterface()` now applies imported interface-method token
  remapping only for dynamic JC305-compatible caller packages. Embedded ROM API
  bytecode keeps its own export method order. Extended the JCAlgTest APDU probe
  with the exact AES-128 CBC-NOPAD static sequence (prepare, five empty-repeat
  baselines, and 50 `doFinal` calls); APDU transcripts now include host elapsed
  milliseconds.
- Validation: `./tools/build arm` passed and produced
  `build/arm/pico2_choupi.uf2`; sequential `./tools/build riscv` passed. Flashed
  ARM to physical Pico `D92E743D5F640C0F`; the initial force-load found no reset
  interface, and `tools/flash` recovered through its CDC-1200 BOOTSEL path,
  then loaded and verified the image at 100%. An 8-second monitor showed
  repeated `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`; it also
  emitted a `WATCHDOG_RECOVERY cause=other` marker, which does not prove a
  watchdog reset. Exact AES probe, SHA probe, and post-run AES probe all passed
  on the physical reader. `git -c core.whitespace=cr-at-eol diff --check` passed.
- Result: Official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` measured SHA-256
  `doFinal()` at 4.04 ms/op and `update()` at 4.38 ms/op (250 invocations
  each). AES-128 CBC-NOPAD measured `Cipher_doFinal()` at 8.58 ms/op and
  `Cipher.init()` at 53.16 ms/op (250 invocations each). Every captured
  performance APDU returned `9000`; the supported cases returned `AA`, and
  unsupported algorithms returned expected `NO_SUCH_ALGORITHM`. The complete
  operation hit the configured 900-second timeout while processing RSA CRT
  public variants: 538 methods started, 537 rows in the generated
  already-measured list, 17 measurement summaries, and 520 expected unsupported
  results. GPPro's CPLC info query returned `6A88` for unavailable data. A final
  focused AES static probe passed after timeout. No other board was flashed.
- Decision: The AES dispatch issue and tested SHA/AES physical paths are
  resolved. The full catalog remains incomplete under the 900-second cap; do
  not treat its timeout or expected unsupported algorithms as VM failures.
  Continue with the next open acceptance item in `TODO.md`.
- Commit: Not created; `.git` is read-only in this environment. No files were
  staged. Pre-existing worktree changes were preserved.
- Artifacts/logs: Official client transcript SHA-256
  `c08bb9016c91aecd14b9b06c5959658ec4f618520d0e845e41f606ecb51611c4` at
  `artifacts/logs/jcalgtest-performance-static-after-aes-remap-20261022.log`;
  partial CSV SHA-256
  `7af3c6b95a4d56c6e4b74afad5e02e6726541a066b8b07548b738bbf6a968efb` and
  already-measured list SHA-256
  `7cb377973ac6fc85e5b9c2dc74edf17f90e4a49e74658352770d083961fc8f21` under
  `artifacts/results/physical-performance-static-aes-remap-20261022/`.
  Focused AES log SHA-256 `e559c60cf322fa5bd7528bed3d3d5d642ac446d549ebd5b367afd097c6a71359`;
  SHA-256 log `ef8b5799276cc166b466a1a2afea725b7fa552792ba491575ea658fda820e90b`;
  post-timeout AES log `e6731dd39cca7be8ef5778b3f6c8e5067b6e3827689245166cd47afe565a05f4`.


## 2026-10-22 — Run current-source deploy coverage across available Pico readers

- Step/experiment: `physical-deploy-two-reader-current-source-20261022`
- Change: Flashed the current ARM image to the two remaining BOOTSEL Pico 2 boards and ran Core, Security, Crypto, and Service deploy areas across the two readers visible through SmartcardIO. Moved the official client's generated already-measured list into the ignored run artifact directory.
- Validation: Both ARM and RISC-V builds had passed before flashing. Both additional boards reported `FLASH:PASS`; each bounded serial monitor reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. PC/SC exposed only two of the three firmware-running boards even after repeating the authorized ACL helper. `run-deploy-parallel --areas Core,Security,Crypto,Service --min-devices 2 --max-devices 2` completed worker tests and cleanup, but exited 1 because optional Core Ext INSTALL returned `6A88`; its rollback passed.
- Result: Security 77 OK, 820 UNSUPPORTED_PER_SPEC, 2 skipped, 2 WRONG_RESULT (dependent on transient AES key construction already classified unsupported in QKB). Core 272 OK, 15 unsupported, 4 Ext applet-not-installed. Crypto 18 OK, 444 unsupported, 2 SW_ERROR, 1 EXC_CARDRUNTIME, 1 WRONG_RESULT. Service 124 OK. Crypto anomalies: `Signature()` and `Cipher()` returned `6F00`; AES CBC-NOPAD `init(..., IV)` returned `CryptoException` reason 5; empty `doFinal` returned 0 where the test expected illegal use. No RSA/other algorithms were added. Third reader `8E464F4E2BBC0A19` enumerated over USB and passed runtime serial checks but remained absent from PC/SC.
- Decision: Core, Security, and Service deployment/lifecycle and cleanup paths ran. Investigate the four Crypto outcomes against CAP test expectations and exact dispatch before changing runtime behavior; retain the transient-key rows as unsupported-feature fallout. Retry three-reader scheduling after PC/SC enumeration is stable.
- Commit: Not created; repository `.git` is read-only in this environment. Existing worktree changes were preserved.
- Artifacts/logs: `artifacts/results/deploy/current-source-two-reader-20261022/`. Pico1 CSV SHA-256 `43b6020993b2daa8b57203a1e74d369e3ece3685b43d19b54f586fd75c9d9252`; pico2 CSV SHA-256 `f2a654d992bc4fcdce5e72583029b2ac17ce3dcade35ee42ef77d21dc73be0bd`.


## 2026-10-22 — Map JC305 Cipher and Signature constructor tokens

- Step/experiment: `physical-crypto-constructor-remap-20261022`
- Change: Extended the package-scoped external constructor token remap in `choupi/src/jc_handlers/jc_cp.cpp` to JC305 `javacard.security.Signature` and `javacardx.crypto.Cipher`, mapping imported constructor token 1 to embedded export token 2. This follows the previously validated MessageDigest mapping and the API export tables.
- Validation: `./tools/build arm` passed (`pico2_choupi.uf2` end `0x100c973c`); sequential `./tools/build riscv` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/flash --serial D92E743D5F640C0F build/arm/pico2_choupi.uf2` passed with 100% verification. Bounded `./tools/monitor --serial D92E743D5F640C0F --seconds 8` reported eight `TEST:PASS choupi_runtime` and eight `TEST:SUMMARY pass=1 fail=0`. An initial scheduler run selected old-image reader 2BED and is excluded from fix validation. Exact-reader physical Crypto area on D92 completed upload, 466 calls, and cleanup successfully.
- Result: `Cipher()` no longer appears as a `SW_ERROR`; `Signature()` still returns `6F00`. Exact Crypto summary: 19 OK, 444 UNSUPPORTED_PER_SPEC, one SW_ERROR, one EXC_CARDRUNTIME, one WRONG_RESULT. Remaining rows: valid AES CBC-NOPAD init-with-IV raises reason 5; empty doFinal returns 0 where the test expected illegal use. Three-board SmartcardIO availability fluctuated from two to one reader despite the ACL helper granting access to three USB nodes.
- Decision: Treat the Cipher constructor mapping as validated. Continue tracing Signature's exact imported token independently; do not assume the same mapping for a class whose behavior did not change. Then investigate the two remaining Cipher API outcomes.
- Commit: Not created; `.git` is read-only. No files were staged.
- Artifacts/logs: `artifacts/results/deploy/crypto-ctor-remap-d92-20261022.csv`, SHA-256 `780f91b12d90f815dc6840a6af8857e859598ea2543a2f1a7a49b4acd1b0d2e`; initial old-image run `artifacts/results/deploy/crypto-ctor-remap-20261022/` is retained but not used to validate the change.

## 2026-10-22 — Map JC305 Signature virtual methods

- Step/experiment: `physical-crypto-signature-vmap-20261022`
- Change: Added a JC305 Security-package, class-token-15 virtual-method map for `javacard.security.Signature`. The Crypto probe's `ApiTestCryptoProbeSignature.getAlgorithm()` override had been reached through the imported Signature method table, whose ordering differs from the embedded CHOUPI table.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed ARM firmware to Pico `D92E743D5F640C0F` with `PICO2_HOST_USB_ACCESS=1 ./tools/flash --serial D92E743D5F640C0F build/arm/pico2_choupi.uf2`; flash verification passed. A 20-second finite monitor showed 20 `TEST:PASS choupi_runtime` and 20 `TEST:SUMMARY pass=1 fail=0`. The exact-reader JC305 Crypto run completed upload, all 466 calls, and cleanup.
- Result: `Signature()` no longer returns `6F00`. Crypto run summary: `UNSUPPORTED_PER_SPEC=444`, `OK=20`, `EXC_CARDRUNTIME=1`, `WRONG_RESULT=1`. The two remaining rows are AES CBC-NOPAD IV initialization (`CryptoException.ILLEGAL_VALUE`) and a zero-length `doFinal` result classified by the official test as unexpected. No additional algorithms were implemented.
- Decision: Keep tracing the AES IV row. Treat the zero-length result as unresolved until CAP and API semantics are reconciled; do not change zero-length `doFinal` behavior based only on the client's label.
- Commit: Not created; repository `.git` is read-only. No files were staged.
- Artifacts/logs: `artifacts/results/deploy/crypto-signature-vmap-d92-20261022.csv`, SHA-256 `5f3eb8024111c4081f049c1b20fe4a3eaf005b07dbd40e79e5616be68839c719`.

## 2026-10-22 — Buffer partial CBC Cipher updates

- Step/experiment: `physical-jc305-cipher-streaming-20261022`
- Change: Implemented `Cipher.update()` partial-block buffering, complete-block processing, and CBC IV chaining in the JC305 API implementation. `doFinal()` combines pending bytes with the final input, throws `ILLEGAL_USE` when a no-padding cipher receives no data, then resets its IV and pending state after success. Aligned calls use the input array directly to avoid a temporary allocation per invocation.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Flashed the ARM image to D92 `D92E743D5F640C0F` and 2BED `2BED26ED20556535`; both reported `FLASH:PASS`. Finite monitors passed on both boards (20 seconds on D92 and 8 seconds on 2BED). The exact-reader Crypto CAP run on D92 completed upload, 466 calls, and cleanup: 22 `OK`, 444 `UNSUPPORTED_PER_SPEC`, zero other statuses. The AES static probe completed prepare, five baselines, and 50 `doFinal()` calls with `AA9000` and `TEST:PASS`; the measurement APDU took 4,114 ms. Five zero-repeat baseline APDUs took 1,229–1,250 ms each.
- Result: The CAP's 20-byte AES `update()` plus 12-byte `doFinal()` path now passes. Empty `doFinal()` returns the expected exception. An initial static attempt with a new temporary buffer on every aligned call returned `6400`; using the existing input buffer for aligned blocks removed that allocation pressure and the 50-call operation passed. PC/SC reader visibility fluctuated; the authorized helper restored D92 before the final runs. The monitor printed a `WATCHDOG_RECOVERY cause=other` diagnostic, but all runtime assertions passed and no `FAULT`/`TEST:FAIL` appeared.
- Decision: Keep this as the supported AES/DES CBC no-padding path. Full `ALG_PERFORMANCE_STATIC` was not rerun; other algorithms remain classified by their own support results.
- Commit: Not created; repository metadata is read-only in this environment. No files were staged.
- Artifacts/logs: `artifacts/results/deploy/crypto-stream-final-d92-20261022.csv`, SHA-256 `467e3c71f26c2a5a927c7e33e34766893899083e34668802d9df6198fd4f3683`; `artifacts/logs/aes-cipher-static-after-streaming-fix-retry-20261022.log`, SHA-256 `8a29e31f1c1702d12dabdb531465b1e66cf795ceb11337929318b771eb2f1096`.

## 2026-10-22 — Recheck current-source Security and Core deploy APIs on hardware

- Step/experiment: `physical-current-source-security-core-20261022`
- Change: Flashed the built ARM image to Pico D92 and 2BED, then distributed JC305 Security and Core deploy areas across the two readers. No algorithm implementations were added for unsupported cases.
- Validation: Both architecture builds passed before flashing. Both flash operations verified successfully. D92's finite monitor passed 20/20 checks during the prior cycle; 2BED's 8-second monitor passed 8/8. Parallel deploy completed Security's 901 calls and Core's 291 calls. Core cleanup passed for the Core and Multi CAPs; optional Ext INSTALL returned `6A88`, and rollback removed the partial CAP successfully. Security worker exit was 0.
- Result: Security: 77 `OK`, 820 `UNSUPPORTED_PER_SPEC`, two intentionally skipped RSA-2048 keypair cases, and two `WRONG_RESULT` follow-up `Key.isInitialized()` checks. Their paired transient AES KeyBuilder calls returned `UNSUPPORTED_PER_SPEC`/`NO_SUCH_ALGORITHM`, so these two checks did not exercise transient-key clearing. Core: 272 `OK`, 15 `UNSUPPORTED_PER_SPEC`, and four Ext methods unavailable because Ext INSTALL was refused. No unexplained Security/Core runtime or wrong-result rows remain in Core.
- Decision: Keep transient-key dependent outcomes classified as unsupported-feature fallout. Keep Ext and logical-channel cases unavailable per the runtime's current supported profile. Current-source Security, Core, and Crypto CAP areas have now all completed on physical readers.
- Commit: Not created; repository metadata is read-only. No files were staged.
- Artifacts/logs: `artifacts/results/deploy/current-source-sec-core-post-cipher-20261022/`. Security CSV SHA-256 `738fd65657523eaa9aca77881f7c194c77bae8161e0ce6af2348ffe697b16686`; Core CSV SHA-256 `b9d90093a17c9ee686ca615d36d90f972201efb79b1a54b0e013317a6ae516a4`; Security log SHA-256 `139e0301da63b867296265d71b08c214d7c64689eecf9d36713be71cf3222775`; Core log SHA-256 `fbf493fb91f68eee14dd9c2e9f3c171fc40d6b0a9e53a69bfedf2e176c70`.

### Streaming Cipher follow-up — in-place decryption IV preservation

- Step/experiment: `physical-jc305-cipher-inplace-iv-20261022`
- Change: CBC decryption now saves the final ciphertext block before processing when input and output are the same array, preserving the correct IV for the next update. It uses the existing block scratch buffer.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The final ARM image flashed and verified on D92 and 2BED; bounded monitors passed on both. The final D92 Crypto CAP run completed all 466 calls: 22 `OK`, 444 expected unsupported, zero unexpected statuses. The AES static 50-call `doFinal()` sequence passed all six preparatory APDUs and the measured APDU (`AA9000`), with measurement elapsed time 4,115 ms.
- Result: Partial-block buffering, CBC chaining, empty-input error behavior, aligned no-allocation execution, and in-place decrypt IV preservation coexist in the final image. Security/Core physical results from the preceding build remain valid for their exercised paths; the final in-place-only revision was reflashed to both boards but those two areas were not rerun after that last small change.
- Decision: Treat the tested JC305 Crypto area and AES static workload as passing on the final image. Keep all other algorithm support classifications unchanged.
- Commit: Not created; repository metadata is read-only. No files were staged.
- Artifacts/logs: `artifacts/results/deploy/crypto-cipher-inplace-final-d92-20261022.csv`, SHA-256 `aeafc4fddf95e785c062db87528ba2b090ce1ef1a7605da64828aef81741d4df`; `artifacts/logs/aes-cipher-static-final-20261022.log`, SHA-256 `fc3c3668e83dcdff0e17a0962077a01cb8ee7430ab1e5ec7ca7a52ba2aad907e`.

## 2026-10-22 — JC305 RMI int-array fixture converter investigation

- Step/experiment: `rmi-int-array-cap-metadata-20261022`
- Change: Added `echoInts(int[])` and `sumInts(int[])` to the CAP 2.2 RMI fixture. Replaced the Ant JavaCard wrapper for this fixture with an explicit JC305 converter invocation using `-i`, since the bundled Ant task does not expose the converter's int-support option.
- Validation: `./tools/build-rmi-array-fixture` passed and emitted a CAP. The Class component inspector still reported only the prior 16 remote methods and no int-array entries. The restricted VPCD attempt failed before startup with `[Errno 1] Operation not permitted`; `./tools/test-rmi-arrays-vpcd` through elevated execution passed the exact revised fixture: 23/23 class-form checks and 3/3 interface-form checks. Verbose conversion confirms int signatures are parsed, but the output metadata still omits them after changing `sumInts` to return `short`. ARM and PC build stages passed as part of that wrapper. RISC-V build and physical flash were not run; no hardware result is claimed.
- Result: JCDK 3.0.5 converter accepts int-enabled conversion and the fixture bytecode, but still omits both int-array remote signatures from CAP Class remote-method metadata. Existing RMI array behavior remains passing; the int-array wire path remains unverified.
- Decision: Keep int-array RMI marked unverified. Next inspect the converter's remote-method metadata generation or construct a valid CAP 2.2 fixture with another emitter before adding int-array APDU expectations.
- Commit: Not created; `.git` metadata is read-only in this environment. No files were staged.
- Artifacts/logs: `build/rmi-array-fixture/remote-methods.txt`, `build/rmi-array-fixture/output/fixture.cap`, and `artifacts/logs/rmi-array-invoke-vpcd-20261022T204723Z.log` (SHA-256 `ad4f71b794689d49b7f710b0e61570ed69653abfb12807f9907ed5e756555f68`) and `artifacts/logs/rmi-reference-interfaces-vpcd-20261022T204723Z.log` (SHA-256 `908719f311d5903ebc3dded6158639049e22efbe236a4744d8e80b69988b0e87`).

## 2026-10-23 — Trace int-array RMI metadata loss to CAP emission

- Step/experiment: `rmi-int-array-cap-emission-trace-20261023`
- Change: Updated the CAP inspector to visit every remote class record. The investigation confirms the generated JCA retains `echoInts(int[])` and `sumInts(int[])` as remote methods, while the resulting CAP Class component has only 16 remote-method records. Updated the working QKB, TODO, and handoff notes with this distinction.
- Validation: `./tools/build-rmi-array-fixture` passed; inspector output contained 16 `RMI_METHOD` records. Direct Converter 3.0.5 invocation with `-out CAP JCA -i` exited 0 and generated JCA containing both int[] method declarations and `.remote` implementations. `git diff --check` passed for the inspector and documentation paths before progress append.
- Result: The methods are parsed and represented in JCA, then omitted during JCA-to-CAP emission. The bundled converter is closed source in this workspace; its exact filtering logic remains unknown. No int[] APDU test is valid until a CAP includes the two metadata records.
- Decision: Keep the guarded VM int[] RMI path unverified. Next use a different CAP emitter or implement a focused CAP Class metadata writer, then add APDU assertions.
- Commit: recorded separately in Git history for this logical step.
- Artifacts/logs: `build/rmi-array-fixture/direct/rmi/fixture/javacard/fixture.jca`, `build/rmi-array-fixture/Class.cap`, `build/rmi-array-fixture/remote-methods.txt`.

## 2026-10-23 — Assemble int RMI metadata and validate array INVOKE

- Step/experiment: `rmi-int-array-invoke-vpcd-20261023`
- Change: Changed the RMI fixture pipeline to have Converter 3.0.5 emit JCA, then run its bundled `capgen` assembler. Added build assertions for `echoInts(int[])` (hash `a336`, signature `dd`), `sumInts(int[])` (hash `bb6c`, signature `d4`), and all 18 remote records. Added APDU cases for short return, signed and nontrivial 32-bit round-trip, empty arrays, and null arrays.
- Validation: `./tools/test-rmi-arrays-vpcd` passed: ARM and PC builds succeeded; class-form VPCD reported `TEST:SUMMARY pass=28 fail=0`, and interface-form reported `pass=3 fail=0`. A standalone fixture build also passed all metadata assertions. No RISC-V build or physical flash was run; this step changed fixture/tooling and exercised the VM through VPCD.
- Result: `capgen` preserves the int-array remote method entries omitted by Converter's direct CAP output. The existing guarded `JCVM_INT_SUPPORTED` RMI argument/result path passes VPCD round-trips, null/empty handling, and primitive short result.
- Decision: Mark int[] RMI end-to-end coverage as validated on VPCD. Physical board coverage remains open; proceed to the next open JCVM priority after reviewing TODO.md.
- Commit: recorded separately in Git history for this logical step.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261023T040059Z.log`; `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T040059Z.log` (SHA-256 `298052464622d6013e4fbd53700e7987defcab052c2ef7333400a8407763900b`); generated CAP and load code under ignored `build/rmi-array-fixture/`.

## 2026-10-23 — Validate int-array RMI on physical Pico 2

- Step/experiment: `physical-rmi-int-array-d92-20261023`
- Change: Added `tools/test-rmi-arrays-physical`, an exact-reader, bounded test that assembles the fixture CAP, refuses to overwrite an existing matching package, installs the unique fixture, sends raw int[] RMI APDUs, uninstalls the fixture, and confirms JCAlgTest remains. GlobalPlatformPro session transcripts stay in ignored build logs; the artifact log contains sanitized step results.
- Validation: `./tools/cycle choupi --serial D92E743D5F640C0F` passed both ARM and RISC-V build gates, flashed current ARM firmware, verified it, and captured repeated `TEST:SUMMARY pass=1 fail=0`. Physical SELECT returned `9000`; `sumInts`, signed 32-bit `echoInts`, empty-array, and null-array APDUs all matched expected responses. Fixture cleanup passed and JCAlgTest SELECT/GETVERSION/JCSystem persistence checks passed afterward.
- Result: The guarded int[] RMI path passes on physical Pico 2 D92, in addition to VPCD. Existing JCAlgTest remained installed; the temporary fixture package was deleted.
- Decision: Mark int[] RMI validation complete for this fixture on VPCD and physical ARM firmware. Keep wider RMI client interoperability and other int-array signatures as open profile coverage.
- Commit: recorded separately in Git history for this logical step.
- Artifacts/logs: `artifacts/logs/rmi-array-physical-20261023T041218Z.log` (SHA-256 `ada9e3747d365338ec3ecd42ee93676b557aee607273634c5bbc1e3bd7c9b583`); `artifacts/logs/cycle-choupi-20261023T040710Z.log` (SHA-256 `7c815169fdea352fea271f55980db1c76c264e84fee5265f81ac298cb6333aa4`); post-cleanup `artifacts/logs/pico-powerup-persistence-20261023T041037Z.log` (SHA-256 `a7c1a4440a9a0261884c942b8d7de5c1cffe3b0eabc625bf810ae413ba136fd8`).

## 2026-10-23 — Cover transitive custom exceptions in RMI

- Step/experiment: `rmi-transitive-custom-exceptions-20261023`
- Change: Extended the RMI fixture's custom checked and runtime exceptions through an applet-defined intermediate superclass, exercising superclass traversal to the nearest Java Card API exception class.
- Validation: `./tools/build-rmi-array-fixture` passed. Elevated `./tools/test-rmi-arrays-vpcd` passed 28/28 class-form APDU checks and 3/3 interface-form checks. Exact responses remained `830900009000` for the runtime branch and `830500009000` for the checked branch. No physical hardware or RISC-V build was needed for this fixture-only change.
- Result: One intermediate applet-defined superclass is handled correctly for both checked and runtime exception branches.
- Decision: Keep longer hierarchy/malformed metadata and complete API reason-code capture open; proceed to the remaining RMI edge coverage.
- Commit: Not created; `.git/index.lock` creation failed because repository metadata is read-only.
- Artifacts/logs: `build/rmi-array-fixture/output/fixture.cap`, `build/rmi-array-fixture/remote-methods.txt`, `artifacts/logs/rmi-array-invoke-vpcd-20261023T055624Z.log`, and `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T055624Z.log`.

## 2026-10-23 — Preserve SystemException reasons in Java Card RMI

- Step/experiment: `rmi-system-exception-reason-20261023`
- Change: The RMI exception mapper now recognizes Java Card `SystemException` before its `java.lang.RuntimeException` superclass. It maps the JC305 framework class token 13 to embedded export token 22 and reads the inherited reason from field slot 0. Extended the CAP fixture and physical probe with `SystemException.throwIt(0x1234)`.
- Validation: `./tools/build arm`, `./tools/build riscv`, and the PC stage in `./tools/test-rmi-arrays-vpcd` passed. VPCD passed 29/29 class-form APDUs and 3/3 interface-form APDUs; the direct throw returned `822512349000`. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial D92E743D5F640C0F` passed the build gate, flash verification, and 18/18 bounded runtime checks. The physical exact-reader probe installed the fixture, passed SELECT/int[]/SystemException calls, removed the fixture, and confirmed JCAlgTest remained installed.
- Result: Java Card SystemException type and its complete 16-bit reason are preserved in the RMI response on VPCD and physical D92.
- Decision: Keep reason capture for other Java Card API exception classes and longer hierarchy cases open. D92 is left running full CHOUPI ARM firmware.
- Commit: Not created; `.git/index.lock` creation failed because repository metadata is read-only.
- Artifacts/logs: `artifacts/logs/rmi-array-invoke-vpcd-20261023T061114Z.log` (SHA-256 `576ae54340d7dd338f010586f589c67bbd0c7c629d7f0fccad002f8cd76b43e0`), `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T061114Z.log` (SHA-256 `eb88b74a57f68a9d411f47371a7d42c8fc2dead8d01fd193cbdafb60a191c37e`), `artifacts/logs/rmi-array-physical-20261023T061632Z.log` (SHA-256 `9f51c9a0888ede4d1d6b3cf9bf6cd3d8fa73ce689287f8bc25b4598f1a53beaf`), and `artifacts/logs/cycle-choupi-20261023T061508Z.log` (SHA-256 `0a75ffe0c9be2cb9a422a824099d67629d83dda9136712e04482355d21adf03d`).

## 2026-10-23 — Preserve Java Card framework exception reasons over RMI

- Step/experiment: `rmi-framework-exception-reasons-20261023`
- Change: Added RMI recognition and reason-field propagation for CardException,
  CardRuntimeException, APDUException, PINException, TransactionException,
  and UserException, complementing SystemException. Corrected ISOException's
  embedded dispatch export token from 18 (CardException) to 21 (ISOException).
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Elevated
  `./tools/test-rmi-arrays-vpcd` passed 35/35 class-form APDUs and 3/3
  interface-form APDUs. The official SDK 2.2.1 JCRMI client identified all
  seven tested framework exceptions correctly. The full CHOUPI ARM image was
  flashed to Pico D92 after the wrapper recovered to BOOTSEL from a missing
  reset interface; flash verification passed and the 18-second monitor emitted
  repeated `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The
  physical fixture returned exact `0x1234` reasons for all seven exceptions,
  cleaned up successfully, and confirmed JCAlgTest remained installed.
- Result: Java Card framework reason preservation and tested exception class
  decoding pass on VPCD, the SDK JCRMI client, and physical ARM hardware.
- Decision: Keep broader custom/framework hierarchy and malformed metadata
  coverage open; continue with remaining Priority 3 RMI profile gaps.
- Commit: `5d3aabc`
- Artifacts/logs: VPCD
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T085319Z.log` (SHA-256
  `b9e137138b2521d639eb009db2a97e76e29b2ea8a915cdf22ca24c1549f60a5b`),
  reference interface VPCD
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T085319Z.log` (SHA-256
  `f1a555116372218ab529344fdc6fb212ee55bda229af3ea02420bd8c8b64085d`),
  official SDK client
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T085436Z.log`
  (SHA-256 `c1eee5645b58ccc0ff7b80b0e47d796db69a24598e059094e3bc71554c1dd572`),
  physical fixture
  `artifacts/logs/rmi-array-physical-20261023T085737Z.log` (SHA-256
  `3043412af0f39865f27277e0a6f3f3625c1fd0dc88b8983a9d5dcaa04cff576e`), and
  cycle `artifacts/logs/cycle-choupi-20261023T085610Z.log` (SHA-256
  `fecca38e10cc1898b92c5ee424515db008285e905a6ddae7fe25ecf319e77041`).

## 2026-10-23 — Preserve CryptoException reasons over RMI

- Step/experiment: `rmi-crypto-exception-reason-20261023`
- Change: Extended the RMI exception hierarchy walk to recognize
  `javacard.security.CryptoException` from the security package class view at
  offset 233 and serialize its 16-bit reason. Added `throwCryptoReason()` to
  the CAP fixture and physical probe (method hash `ec5c`).
- Validation: Elevated `./tools/test-rmi-arrays-vpcd` passed 36/36 class-form
  and 3/3 interface-form APDUs; CryptoException returned `823012349000`.
  The official SDK 2.2.1 client decoded it as
  `javacard.security.CryptoException`. `./tools/build arm` and
  `./tools/build riscv` passed. Full CHOUPI ARM flashed and verified on D92;
  bounded monitor passed repeated runtime summaries. Physical fixture returned
  `823012349000`, cleaned up, and confirmed JCAlgTest remained installed.
- Result: Both native crypto errors and thrown Java CryptoException objects
  have correct RMI type handling; this fixture confirms Java reason-code
  propagation on VPCD and physical hardware.
- Decision: Keep remaining security-package exception types/subclasses and
  malformed hierarchy coverage open; move to the next RMI profile gap.
- Commit: `5d3aabc`
- Artifacts/logs: VPCD
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T090126Z.log` (SHA-256
  `d35b03c76aa43980ffa8e0b3272457a4f753d51c7d62dc64bc4ba8f32ef57b85`),
  interface VPCD
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T090126Z.log` (SHA-256
  `080a4f5a4371bf54d36ee960b5e0e969e0a85be8e12a86d9c5c934b0f066cb04`),
  official SDK client
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T090250Z.log`
  (SHA-256 `65bdfcf5f0f2d3e891832ea1314c2a224fd60b1657eaf8f618495f48aafe0db4`),
  physical fixture
  `artifacts/logs/rmi-array-physical-20261023T090545Z.log` (SHA-256
  `e253d1e54f36011f05c6504d90b5dfa6938f5b295d7c74544554cc8e578e1063`), and
  cycle `artifacts/logs/cycle-choupi-20261023T090418Z.log` (SHA-256
  `86eb26d671e4795a6efc504bbfefcc2610136a2dc82f4188e8631e0cfb7b36c5`).

## 2026-10-23 — Validate RMI reason mapping through CryptoException subclasses

- Step/experiment: `rmi-custom-crypto-exception-hierarchy-20261023`
- Change: Added an applet-defined `CustomCryptoException` subclass and a remote
  method that throws it with reason `0x1234`. The expected wire response is
  `833012349000`, mapping the custom class to its nearest exported API parent.
- Validation: Elevated VPCD suite passed 37/37 class-form and 3/3
  interface-form APDUs. The SDK 2.2.1 proxy decoded it as its
  `CryptoExceptionSubclass` wrapper. Physical D92 returned exact
  `833012349000`; fixture cleanup and JCAlgTest preservation passed. No VM
  source changed after the preceding ARM/RISC-V builds and physical flash.
- Result: Cross-package exception hierarchy traversal preserves the framework
  subclass marker and inherited 16-bit reason on VPCD, SDK client, and hardware.
- Decision: Keep longer inheritance chains and other API families open; this
  covers one applet-defined subclass of a Java Card security exception.
- Commit: `5d3aabc`
- Artifacts/logs: VPCD
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T090847Z.log` (SHA-256
  `63468996fb3d35b42b6c73da3f2c3513f2215f0798bc55a058484fd4a75caf25`),
  interface VPCD
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T090847Z.log` (SHA-256
  `448b2df34db7b0a0592bc955342fc70e7b0bf2decb388b57c2b5f38aeceee98d`),
  SDK client
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T091002Z.log`
  (SHA-256 `7272879d6badf19165e0fe27810a139821f38f719fe6341747a1030ebed36abe`),
  and physical fixture
  `artifacts/logs/rmi-array-physical-20261023T091115Z.log` (SHA-256
  `1ba3269bc978203104971fec36331bfb3ee53d2bd6b9dbe4d8340ced34c33271`).

## 2026-10-23 — Extend RMI CryptoException hierarchy depth

- Step/experiment: `rmi-crypto-exception-depth-two-20261023`
- Change: Added an applet-defined `CustomCryptoExceptionBase` between the
  remote leaf exception and Java Card `CryptoException`, exercising a longer
  package-crossing hierarchy walk. The remote method hash remains `8a9e`.
- Validation: Elevated VPCD passed 37/37 class-form and 3/3 interface-form
  APDUs; the official SDK 2.2.1 client decoded the `833012349000` response as
  its `CryptoExceptionSubclass` wrapper. Physical D92 returned the exact same
  response; fixture cleanup and JCAlgTest preservation passed. This was a
  fixture-only change, so firmware was not rebuilt or reflashed.
- Result: RMI mapping preserves the custom-subclass marker and inherited
  reason through two applet-defined classes before the exported API parent.
- Decision: Longer chains, malformed hierarchy data, and other API exception
  families remain open.
- Commit: `f755e9b`
- Artifacts/logs: VPCD
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T094919Z.log` (SHA-256
  `185618b77deb954cbcfca091dbc45db7ccf69c20b8021e7183fb2b3c56c0e06e`),
  interface VPCD
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T094919Z.log` (SHA-256
  `ef05a35ca1692b54f6d2b86dfaf416113ecda6b6e83bf38e7f01aeb185c8a3d2`),
  SDK client
  `artifacts/logs/rmi-client-official-vpcd-emulator-20261023T095024Z.log`
  (SHA-256 `00e0bebeef01e5b3e824d25e082a6412d563636c46c26749437c17b8cb451c93`),
  and physical fixture
  `artifacts/logs/rmi-array-physical-20261023T095138Z.log` (SHA-256
  `1ba3269bc978203104971fec36331bfb3ee53d2bd6b9dbe4d8340ced34c33271`).

## 2026-10-23 — Reject truncated and oversized declared RMI int[] payloads

- Step/experiment: `rmi-int-array-payload-bounds-20261023`
- Change: Added APDU regressions for a truncated int[] (declares five elements,
  provides one) and a hostile declared length of 254 with only one element.
  Both must be rejected before array allocation with RMI error `9900039000`.
- Validation: `./tools/test-rmi-arrays-vpcd` passed 39/39 class-form and 3/3
  interface-form APDUs. The two new errors were followed by valid int[] calls,
  which passed. The exact-reader physical D92 probe returned `9900039000` for
  both malformed requests; subsequent arrays and exception cases passed,
  cleanup succeeded, and JCAlgTest remained installed. Fixture/test-only
  change; no firmware rebuild or flash was needed.
- Result: Truncated and overdeclared int[] byte counts are rejected without
  breaking later RMI calls on VPCD or physical hardware.
- Decision: Keep malformed signature and other primitive-array boundary cases
  open.
- Commit: `14e2f7f`
- Artifacts/logs: VPCD
  `artifacts/logs/rmi-array-invoke-vpcd-20261023T102107Z.log` (SHA-256
  `b6b4e38ef19c2a56215fd9ecb39fee62ed76d696e414aaf6a7e48b18d0984ba8`),
  interface VPCD
  `artifacts/logs/rmi-reference-interfaces-vpcd-20261023T102107Z.log` (SHA-256
  `9e03d72a589b600263b5152cb7ba9cce66c46985353d6c300245c73b26738f7d`), and
  physical fixture
  `artifacts/logs/rmi-array-physical-20261023T102244Z.log` (SHA-256
  `8b1c3136d3876f95325baf826a5398d4f0773a425b9324a6ddf46b4e299f3004`).

## 2026-10-23 — Run JC305 CAPs across three physical Pico 2 tokens

- Step/experiment: `deploy-jc305-three-token-parallel-20261023`
- Change: Completed the full API 3.0.5 CAP split on three exact-reader-pinned Pico 2 tokens, then compared Security across all three and repeated Service concurrently and on D92 alone. The artifact summary includes per-token tables and paths.
- Validation: `PICO2_HOST_USB_ACCESS=1 ./tools/run-deploy-parallel --api 305 --min-devices 3 --max-devices 3 --out-dir artifacts/results/deploy/parallel-20261023-three-token-clean-1234` completed every worker (worker 3's area-level install failures made the coordinator return 1). Security was repeated on all three; Service was repeated on all three and then solo on D92. Final elevated dry run found all three readers.
- Result: Security counts matched exactly on all three: 901 calls, 77 OK, 820 unsupported, 2 skipped, 2 `WRONG_RESULT`. Crypto 22/466 OK, JSec 84/84, and Service 124/124 passed on token 2; Core 272 OK / 15 unsupported / 4 Ext methods unavailable. Core optional Ext and Javacardx, JavacardxExt, JavacardxInt, Biometry INSTALLs returned `6A88` on tested tokens; all partial LOADs rolled back. Service on D92 reproduced 12 `SW=6400` Dispatcher errors in concurrent (113/124 then 112/124 on repeat) and solo (112/124) runs. No token disconnected; upload and cleanup passed. Exact firmware identity across tokens was not checked. A subset Security invocation exposed that empty worker buckets run all areas; see QKB and HANDOFF.
- Decision: Treat D92 Service `SW=6400` as reproducible device-specific behavior pending firmware identity comparison and Dispatcher investigation. Fix the parallel runner's empty-area semantics before future subset runs.
- Commit: `9d8d8fc`.
- Artifacts/logs: `artifacts/results/deploy/parallel-20261023-three-token-clean-1234/` (including `SUMMARY.md`), `artifacts/results/deploy/parallel-20261023-security-three-token-1240/`, `artifacts/results/deploy/parallel-20261023-service-three-token-1255/`, and `artifacts/results/deploy/service-token3-solo-20261023.csv`.

## 2026-10-23 — Reflash all tokens and run the full JC305 CAP suite

- Step/experiment: `deploy-jc305-reflashed-full-suite-three-tokens-20261023`
- Change: Rebuilt both architectures, reflashed the full CHOUPI ARM image to all three exact physical serials, and ran every API 3.0.5 area and Core scenario on each token. The suite included RSA-2048 cases.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial <serial>` returned `CYCLE:PASS` for all three devices, with verified 100% image loads and 18-second passing monitors. Exact-reader full suites completed with exit 0 on all three. Final elevated dry run found 3 PC/SC readers.
- Result: Each token completed 1,866 calls. Tokens 1/2 each: 579 OK, 1,281 expected unsupported, 4 `APPLET_NOT_INSTALLED` (optional Ext methods), 2 `WRONG_RESULT`. Token 3: 568 OK, 1,281 expected unsupported, 4 Ext-dependent unavailable, 2 wrong, and 11 Service `SW_ERROR` (`SW=6400`). The 11 Dispatcher failures remain only on D92 after all three boards were flashed with the same UF2 SHA-256 `0945dd18ce32477b82c8f224a5f1c51f45cfe3b62a6a413809de629d354b938a`. All successful CAP installs cleaned up and every partial LOAD rolled back. Optional installs returned the same `6A88` on all boards. Each boot monitor emitted one retained `WATCHDOG_RECOVERY cause=other` marker alongside repeated passing runtime summaries; this marker is already documented as non-failure by itself.
- Decision: Firmware image mismatch is ruled out as the cause of D92's Service difference. Investigate persistent runtime/card state and Dispatcher behavior; do not reopen expected unsupported algorithms.
- Commit: `6763690`.
- Artifacts/logs: `artifacts/results/deploy/full-20261023-three-reflashed-all-areas-1315/SUMMARY.md`, per-token CSV/log files in that directory, and flash/monitor logs `artifacts/logs/cycle-choupi-20261023T110123Z.log`, `artifacts/logs/cycle-choupi-20261023T110257Z.log`, `artifacts/logs/cycle-choupi-20261023T110419Z.log`.

## 2026-10-23 — Record selectable Java Card capability work package

- Step/experiment: `selectable-javacard-capabilities-plan-20261023`
- Change: Added the complete 15-capability configuration, provider extraction, native modularization, expected-CSV, size-inspection, and three-configuration physical acceptance checklist to root `TODO.md`.
- Validation: Inspected the current 15-entry JCAlgTest support declaration, comparator, API sources, ROM-mask build flow, Pico CMake target, and native implementation locations. No build was run for this documentation-only step.
- Result: The work package and key integration points are recorded. Existing dirty API submodule state must be preserved.
- Decision: Start with strict config parsing/generation and build-flow integration, then move algorithm providers and native modules behind the config.
- Commit: `ddc7560` (TODO placement correction).
- Artifacts/logs: `TODO.md`, `.agent/HANDOFF.md`; none.

## 2026-10-23 — Add Java Card capability configuration generator

- Step/experiment: `selectable-javacard-capability-contract-20261023`
- Change: Added the 15-boolean default `config/javacard-support.json`, strict
  validation and deterministic CSV/CMake/header/JSON generation in
  `tools/javacard-support.py`. The JCAlgTest comparator now accepts an empty
  expected-support file. Added focused tests for default, minimal, invalid, and
  comparator cases.
- Validation: `python3 tools/test-javacard-support.py` passed 4 tests;
  generated default CSV exactly matched
  `tools/jcalgtest-supported-v1.8.2.csv`. Initial test attempts exposed
  non-executable helper scripts and a malformed synthetic CSV; the test runner
  now invokes Python explicitly and uses the client's section-row format.
- Result: The generator emits exactly 15 support rows for current support and
  a header-only expected CSV for minimal support. Unknown/missing/non-boolean
  keys and AES cipher without AES key are rejected. No firmware or hardware
  validation was required or run in this tooling-only step.
- Decision: Keep the checked-in 15-row support declaration as a parity
  reference while making the build pass the generated CSV explicitly; wire
  configuration outputs into both build flows next.
- Commit: `bcef65f`.
- Artifacts/logs: `tools/test-javacard-support.py`; generated scratch output
  under `/tmp/jc-support-current`.

## 2026-10-23 — Wire support config into both build wrappers

- Step/experiment: `selectable-javacard-support-build-invalidation-20261023`
- Change: Both root and nested CMake include generated capability metadata.
  `tools/build-pc` creates that metadata before configuring; the root build
  wrapper checks the JSON hash and rebuilds PC API/ROM-mask/JNI outputs when it
  changes. PC API and ROM-mask custom commands depend on the config. The
  comparator now defaults to the generated expected CSV.
- Validation: `bash -n tools/build tools/build-pc` and the capability test
  suite passed. `./tools/build arm` and `./tools/build riscv` both passed;
  full ARM CHOUPI UF2 ends at `0x100c9900`, and RISC-V core compilation passed.
  ROMMASK generated `build/choupi-pc/flash` and `jni.hpp`; checksum validation
  passed for both plus API and support-config manifests. No device was flashed.
- Result: Both architectures use the same validated selection metadata, and
  a config hash change invalidates generated ROM-mask/JNI state. Providers and
  native code are not conditional yet, so image-size differences are not
  expected at this stage. ROM-mask build emitted existing Maven and sandbox
  hostname warnings but completed successfully; ARM/RISC-V compile steps were
  warning-free.
- Decision: Proceed to route Java API factories through selectable providers,
  then condition native source/module inclusion and test all three profiles.
- Commit: `3d583bc`.
- Artifacts/logs: `build/choupi-pc/generation.sha256`,
  `build/choupi-pc/support-config.sha256`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Extract secure random Java provider

- Step/experiment: `selectable-secure-random-provider-20261023`
- Change: `RandomData.getInstance` now delegates to generated provider
  dispatch. The public `RandomData` base no longer contains random generation
  or native calls. `SecureRandomData` is a separate provider template emitted
  only when `secure_random` is enabled; minimal output omits its source file.
  The nested CMake API build compiles generated Java sources into the ROM mask.
- Validation: `python3 tools/test-javacard-support.py` passed 4/4, including
  presence/absence of secure-random source by config. `./tools/build arm` and
  `./tools/build riscv` passed; the API compiled 139 Java sources and ROMMASK
  generated the API/JNI outputs. ARM full image ended at `0x100c9900`. No
  physical flash was performed.
- Result: Selected secure random behavior is compiled as provider bytecode;
  disabled secure random has only an unsupported factory path. The other 14
  Java algorithm/PIN providers and native implementations remain to extract.
- Decision: Continue through key and cipher provider extraction, then digest,
  PIN variants, and native modules; test reduced ROM masks and CSV sweeps.
- Commit: `d6a9157` (API submodule `4541b95`).
- Artifacts/logs: `build/choupi-pc/javacard-api`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Extract AES and DES key providers

- Step/experiment: `selectable-symmetric-key-providers-20261023`
- Change: `KeyBuilder.buildKey` now delegates to a generated dispatch class.
  Generic key storage and AES/DES key implementations moved out of the public
  `KeyBuilder` source into separate provider templates. The generator emits
  only selected AES and/or DES provider classes; key lengths are individually
  selected in dispatch.
- Validation: Focused generator suite passed 5/5, including current, no-AES,
  and minimal provider presence/absence checks. With current support,
  `./tools/build arm` and `./tools/build riscv` passed. API compilation used
  143 source files; ROMMASK regenerated and Pico full ARM image built. No
  hardware flash was performed.
- Result: Selected key providers are separated from the public class, and
  disabled AES provider source is omitted for the no-AES profile. Cipher,
  SHA-256, PIN, and native module extraction remain open.
- Decision: Next route Cipher factory selection through providers and keep
  AES core support needed by SCP03 separate from Java Card AES capability.
- Commit: `591751c` (API submodule `420c82a`; includes prior provider commit
  `4541b95`).
- Artifacts/logs: `build/choupi-pc/javacard-api`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Extract selectable CBC cipher providers

- Step/experiment: `selectable-cbc-cipher-providers-20261023`
- Change: Public `Cipher.getInstance` signatures now dispatch through generated
  capability-specific factories. The inline AES/DES CBC implementation was
  extracted into a shared CBC processing provider with separate AES and DES
  subclasses. Generation emits only the selected cipher-family subclass and
  omits all CBC processing sources for minimal support.
- Validation: `python3 -m py_compile tools/javacard-support.py
  tools/test-javacard-support.py` and the 5-test support suite passed.
  `./tools/build arm` and `./tools/build riscv` passed after API conversion;
  API compilation used 147 sources, ROMMASK output was 36,547 bytes and JNI
  regenerated, and full ARM image linked through `0x100c9900`. A first compile
  caught a dropped Javadoc opener during extraction; it was restored before
  the passing build. No physical device was flashed.
- Result: Java Card AES/DES cipher implementations can be independently left
  out of generated API ROM masks. SCP03 AES remains in its separate firmware
  source path. Native AES/DES code is still combined in `jni.cpp`.
- Decision: Continue with SHA-256 and OwnerPIN providers, then extract native
  crypto modules and verify reduced-profile ROM and image sizes.
- Commit: `282b163` (API submodule `f327c66`).
- Artifacts/logs: `build/choupi-pc/javacard-api`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Extract SHA-256 Java provider

- Step/experiment: `selectable-sha256-provider-20261023`
- Change: `MessageDigest.getInstance` now uses generated provider dispatch.
  The retainable SHA-256 implementation lives in a separate provider source
  emitted only when `sha256` is enabled. The public API class no longer embeds
  SHA-256 native bridge calls; its unsupported OneShot methods retain their
  signatures without referencing the optional SHA bridge.
- Validation: Python compilation and support generator tests passed 5/5.
  `./tools/build arm` and `./tools/build riscv` passed. API compiled 149 Java
  sources; ROMMASK/JNI regenerated with a 36,452-byte ROM image; full ARM image
  linked through `0x100c9900`. No physical flash was performed.
- Result: SHA-256 implementation bytecode is controlled by one boolean and
  support CSV generation follows that selection. Native SHA-256 routines are
  still in `jni.cpp` and remain to be split.
- Decision: Next extract optional OwnerPIN variants, then split all selectable
  native algorithms and libraries while retaining SCP03 AES.
- Commit: `cde592d` (API submodule `f6dab0e`).
- Artifacts/logs: `build/choupi-pc/javacard-api`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Extract selectable OwnerPIN providers

- Step/experiment: `selectable-ownerpin-providers-20261023`
- Change: `OwnerPINBuilder` now dispatches through generated capability logic.
  Optional OwnerPIN X base/implementation sources are kept as provider
  templates and emitted only when their corresponding flags are selected.
  Disabled factory branches retain public signatures and throw
  `SystemException.ILLEGAL_USE`; invalid type values retain
  `PINException.ILLEGAL_VALUE`.
- Validation: `python3 -m py_compile tools/javacard-support.py
  tools/test-javacard-support.py` and `python3 tools/test-javacard-support.py`
  passed 5/5. `./tools/build arm` and `./tools/build riscv` both passed on the
  all-enabled config; ROMMASK/JNI regenerated and full ARM image linked
  through `0x100c9900`. No physical flash was performed.
- Result: Selectable PIN bytecode is separated from public API sources. All
  15 generated dispatch selections now have provider behavior; native
  algorithms and the three alternate build profiles remain.
- Decision: Continue extracting selectable native functions and conditionally
  compiling their modules/libraries, while preserving SCP03 AES.
- Commit: `d1e1529` (API submodule `727767c`).
- Artifacts/logs: `build/choupi-pc/javacard-api`, `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`,
  `build/riscv/pico2_jcvm_smoke.uf2`.

## 2026-10-23 — Split optional native modules and validate profiles

- Step/experiment: `selectable-native-modules-profiles-20261023`
- Change: RandomData, CBC AES, CBC DES, and SHA-256 implementations now live
  in separate native source modules. The two cipher backends are independent;
  PC and Pico CMake select the same modules from the generated support file.
  Java optional native methods were moved from always-present
  `NativeImplementation` into generated bridge classes, allowing ROMMASK/JNI
  generation to remove their entry points with the capability. PC and Pico
  omit Mbed TLS DES/SHA and Pico SHA modules when disabled. AES and Pico random
  remain linked because SCP03 still uses them for card management.
- Validation: Python generator tests passed 5/5 and the current generated
  15-row expected CSV matched `tools/jcalgtest-supported-v1.8.2.csv` exactly.
  `./tools/build arm` and `./tools/build riscv` passed for current support,
  no-Java-Card-AES (10 selected), and minimal (0 selected). Reduced profiles
  were selected only by editing `config/javacard-support.json`. The minimal
  JNI header had no optional bridge methods; its Ninja rules contained no
  optional native modules, Mbed TLS DES/SHA source, or Pico SHA target. PC
  ROMMASK checks reported imported CAP packages valid in all profiles.
- Result: ROM-mask active content / fixed `flash` container / ARM binary /
  ARM UF2 sizes were: current 36,574 / 524,300 / 825,560 / 1,651,712 bytes;
  no-AES 36,169 / 524,300 / 824,576 / 1,649,664 bytes; minimal 32,491 /
  524,300 / 812,792 / 1,626,112 bytes. No-AES and minimal active masks and
  images are smaller than current. The support JSON is restored to the
  all-enabled current profile. These measurements are build-only; no image
  was flashed and GPPro/JCAlgTest physical acceptance is still pending.
- Decision: Keep SCP03 AES and Pico random in all builds; both are required by
  card management independently of Java Card AES/RandomData capabilities.
  Next identify an available physical Pico through the host-access path, then
  test current, no-AES, and minimal images with GPPro and JCAlgTest. Confirm
  the reports against each profile's generated CSV.
- Commit: pending (API submodule `9d9ab43`).
- Artifacts/logs: `/tmp/jc-cap-current-arm.log`,
  `/tmp/jc-cap-current-riscv.log`, `/tmp/jc-cap-no-aes-arm.log`,
  `/tmp/jc-cap-no-aes-riscv.log`, `/tmp/jc-cap-minimal-arm.log`,
  `/tmp/jc-cap-minimal-riscv.log`, `/tmp/jc-cap-current-final-arm.log`,
  `/tmp/jc-cap-current-final-riscv.log`; generated `build/choupi-pc/flash`,
  `build/choupi-pc/jni.hpp`, `build/arm/pico2_choupi.uf2`.

## 2026-10-23 — Reflash reachable Pico 2 tokens and rerun API 3.0.5 suite

- Step/experiment: `full-cap-suite-two-token-reflash-20261023`
- Change: Rebuilt the all-enabled current firmware and completely reflashed
  each Pico 2 currently exposed to this VM, using exact USB serials. Both
  images passed picotool verification and bounded runtime monitoring.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. Elevated
  host-access cycles passed for `2BED26ED20556535` and
  `D92E743D5F640C0F`, including `FLASH:PASS`, repeated
  `TEST:PASS choupi_runtime`, and `TEST:SUMMARY pass=1 fail=0`. Full API
  3.0.5 runs then covered all nine areas on each reader, with the runner's
  default RSA-2048 exclusion disabled. GPPro cleanup or rollback succeeded
  for every successfully installed CAP and every refused optional install.
- Result: In both CSVs Core had 272 OK, 15 expected unsupported, and 4
  Ext-dependent applet methods unavailable; JavacardxSec passed 84/84.
  Security matched exactly (702 unsupported, 106 runtime exceptions, 79
  `SW=6F00`, 6 wrong results, 4 card-runtime exceptions, 4 OK); Crypto matched
  exactly (256 unsupported, 192 `SW=6F00`, 7 runtime exceptions, 11 OK).
  Service passed 124/124 on 2BED but had 11 `SW=6400` of 124 on D92. Core's
  Ext install and the Javacardx, JavacardxExt, JavacardxInt, and Biometry CAPs
  were refused at INSTALL with `6A88`; partial uploads were rolled back.
  Only two RP2350 USB devices and two Pico PC/SC readers were visible; the
  third token (`8E464F4E2BBC0A19`) could not be reflashed or tested in this
  VM. UF2 SHA-256: `d4fd102051767fce3ed72abeac3eef581c579b0e63b20048f1040b80fc364b57`
  (1,652,736 bytes).
- Decision: Treat D92's Service `SW=6400` as a reproduced token-specific
  failure. The broad Security/Crypto `SW=6F00` results are identical across
  the two tokens and need investigation; this unrestricted run differs from
  earlier runs that excluded RSA-2048. Retry the third token when visible and
  compare after identifying the Security/Crypto failing call paths.
- Commit: `ea095bc`.
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261023T141401Z.log`,
  `artifacts/logs/cycle-choupi-20261023T141535Z.log`,
  `artifacts/results/deploy/full-305-2BED26ED20556535-20261023.csv`,
  `artifacts/results/deploy/full-305-D92E743D5F640C0F-20261023.csv`.

## 2026-10-23 — Physical JCAlgTest capability-profile comparison attempt

- Step/experiment: `jcalgtest-current-minimal-physical-20261023`
- Change: Matched JC305 security class-token mappings by exported class identity and routed every static-field bytecode through imported static-field resolution. Root support config restored to all 15 enabled.
- Validation: `./tools/build arm` and `./tools/build riscv` passed for the minimal and restored current profiles. Exact Pico `2BED26ED20556535` cycles passed flash verification and finite runtime monitor for current, minimal, and restored current images.
- Result: Fresh supplied JCAlgTest CAP SELECT returned `9000` on both profiles. GETVERSION returned `F103` current and `6F00` minimal. No valid support CSV was produced; no AES/SHA support difference can be claimed. The candidate class/static-field fixes did not resolve initialization.
- Image measurement: Restored current UF2 is 1,653,248 bytes (SHA-256 `5ddd7cba3543e410e1419d43bc2cc53ac1b8b273e9b4f725a46cdbe8f2d74422`). Minimal UF2 hash/size were not preserved before restoring current.
- Decision: Preserve the comparison as blocked on JCAlgTest eager initialization; next isolate the constructor/provider causing current `NO_SUCH_ALGORITHM` and minimal `6F00`, then rerun full support reports and compare against generated profile CSVs.
- Commit: `fae35b1`; existing unrelated worktree changes preserved.
- Artifacts/logs: `artifacts/results/jcalgtest-capability-profile-20261023/physical-comparison.md`, `artifacts/logs/jcalgtest-current-version-probe-20261023.log`, `artifacts/logs/cycle-choupi-20261023T153233Z.log`, `artifacts/logs/cycle-choupi-20261023T153724Z.log`, `artifacts/logs/cycle-choupi-20261023T154235Z.log`.

## 2026-10-23 — Firmware and JCVM architecture guide

- Step/experiment: `architecture-code-navigation-20261023`
- Change: Added `FIRMWARE_JCVM_ARCHITECTURE.md`, tracing the active RP2350 ARM boot, CCID/APDU dispatch, VM/native calls, AES CBC, object memory, persistence, and debugging entry points; distinguished inactive STM32/Rust OS and RISC-V smoke paths.
- Validation: Inspected active CMake targets and corresponding boot, transport, interpreter, native crypto, heap, and flash source; checked four required sections and 1,653-word length. Documentation-only step; firmware builds and hardware validation were intentionally skipped.
- Result: Guide SHA-256 `494f8fba55f4417f69f63432985f3311a3bd316ec17ce43b6b1c3a563103e23e`; no runtime metric was measured.
- Decision: Use the guide as an onboarding map, and recheck generated bindings and selected capability profile before modifying implementation.
- Commit: `d683cb8` (guide and initial progress record; SHA recorded by this follow-up commit).
- Artifacts/logs: `FIRMWARE_JCVM_ARCHITECTURE.md`; no test logs.


## 2026-10-23 — Minimal-profile Java exception mapping

- Step/experiment: `minimal-jcalgtest-uncaught-6f00-20261023`
- Change: Matched JC305 `javacard.framework` and `javacard.security` class tokens to embedded export tokens by class name. Corrected CryptoException recognition to security export token 26 and ISOException uncaught translation to framework export token 18. Removed this step’s temporary invoke/range tracing after diagnosis.
- Validation: `./tools/build arm` and `./tools/build riscv` passed with the 0/15 minimal profile and again with the restored 15/15 current profile. Minimal ARM was flashed to Pico 2 `D92E743D5F640C0F`; exact package-AID SELECT returned `9000` and GETVERSION changed from `6F00` to `F103`. The surfaced `F103` is the applet’s translation of `CryptoException.NO_SUCH_ALGORITHM` (reason 3), not an uncaught VM exception. All-enabled ARM image was then flashed to D92; finite eight-second monitor passed (`TEST:SUMMARY pass=1 fail=0`). A final SmartcardIO probe of the all-enabled image was blocked because D92 remained USB-visible but PC/SC temporarily enumerated only the other two readers.
- Result: The minimal-profile `6F00` is fixed. JCAlgTest still does not complete initialization under minimal capabilities because its `AlgStorageTest` constructor unconditionally requests AES/DES engines and keys; thus no complete support CSV was produced and AES/SHA report comparison remains outstanding.
- Decision: Keep all-enabled support config restored. Next decide how to make the unchanged JCAlgTest fixture tolerate deliberately absent algorithms, then run its complete support query in both profiles.
- Commit: `d229f2f`
- Artifacts/logs: minimal probe output was captured in the session transcript; all-enabled finite monitor `artifacts/logs/minimal-exception-translation-monitor-20261023.log` (ignored by Git), SHA-256 `5a8a5b5abcccd8acc28aa4c677966c26c4be208675d252a54292cb69b2c02c2b`; all-enabled UF2 SHA-256 `6641567323dcaf0d8cf1ba0e6c3108f1e9235fd144cb466a8ba9b6e00d0a2efc`.

## 2026-10-23 — Archive malformed CAP code review

- Step/experiment: `malformed-cap-review-archive-20261023`
- Change: Saved the source review of malformed CAP crash and stall paths at `todo/review_20261023.md`.
- Validation: Rechecked the cited source paths, confirmed the file is 826 words, and checked its whitespace. Documentation-only step; build, VPCD, fuzzing, and hardware validation were intentionally skipped.
- Result: Five prioritized findings and a remediation/test plan recorded; no runtime metric measured. Review SHA-256 `4ecc4b96a719691671f2f2ac1256a3a67460b184f0060b3d48f11c80b90d01b0`.
- Decision: Keep the findings as source-review hypotheses until malformed fixtures reproduce or refute each path; begin fixes at LOAD-time structural validation.
- Commit: `0daf5bf` (review and initial progress record; SHA recorded by this follow-up commit).
- Artifacts/logs: `todo/review_20261023.md`; no test logs.
## 2026-10-24 — Physical JCAlgTest base/current capability comparison

- Step/experiment: `jcalgtest-base-current-physical-20261024`
- Change: Added the working `base` profile (10 of 15 selected). The three
  OwnerPINBuilder providers remain enabled because JCAlgTest 1.8.2 catches
  `CryptoException` for capability probes but not the documented
  `SystemException.ILLEGAL_USE` for disabled PIN variants. Corrected CCID LOAD
  rejection so failed E8 blocks return `6A80` instead of generic `6D00`.
  Updated the runbook and status/knowledge records.
- Validation: Current ARM/RISC-V wrappers and sequential base ARM/RISC-V
  wrappers passed. An initial concurrent ARM/RISC-V attempt raced in the
  shared PC build tree; the ARM failure was resolved by rerunning the wrappers
  sequentially before flashing. Current and final base ARM images were flashed
  to Pico D92 (`D92E743D5F640C0F`); each bounded runtime monitor passed. GPPro
  clean LOAD+INSTALL succeeded on base before PIN providers were enabled; the
  installed CAP remained selectable across reflashes. Complete physical
  `ALG_SUPPORT_EXTENDED` runs on base and current each matched 4,525 row
  identities and passed the generated-support comparator. A final duplicate
  LOAD test returned the expected `6A80`; GPPro LIST and the follow-up physical
  SELECT/GETVERSION/JCSystem probe confirmed the existing applet remained
  installed and operational. `git diff --check` passed.
- Result: Base reports 10 `yes`, 4,515 `no`, zero other statuses. Current
  reports 15 `yes`, 4,510 `no`, zero other statuses. Current adds only
  AES-192, AES-256, DES, DES3-2KEY, and SHA-256. The same D92 unit was used for
  both profiles. Base UF2 is 1,652,736 bytes; current image size was not
  preserved. Both test logs contain 4,525 `B0 E2` reset APDUs returning
  `6F00`; JCAlgTest does not catch `SystemException.ILLEGAL_USE` from the
  deliberately unsupported `JCSystem.requestObjectDeletion()`, but all
  following capability result rows are valid.
- Decision: Keep base at 10 capabilities so the official applet emits strict
  yes/no rows while omitting five optional crypto capabilities. Retain the
  `6A80` response for duplicate or malformed LOAD data. The prior seven-flag
  base trial is superseded.
- Commit: `744bceb` (implementation and comparison); this follow-up records the
  final progress-record SHA.
- Artifacts/logs: `artifacts/results/jcalgtest-profile-comparison-20261024.md`,
  `artifacts/results/jcalgtest-profile-base10-20261024/`,
  `artifacts/results/jcalgtest-profile-current-20261024/`,
  `artifacts/logs/jcalgtest-profile-comparison-20261024.log`,
  `artifacts/logs/base10-monitor-20261024.log`,
  `artifacts/logs/cycle-choupi-20261023T222949Z.log`,
  `/tmp/duplicate-load-base10.log`.

## 2026-10-24 — Selectable profile acceptance with upstream and deploy CAPs

- Step/experiment: `selectable-profiles-acceptance-20261024`
- Change: Recorded acceptance results and limits; made profile-dependent
  ISOException ROM lookup derive its offset from the exported class location;
  added an optional JCSystem-info skip to the APDU probe; corrected the base
  profile test to expect its three enabled OwnerPINBuilder capabilities.
- Validation: Current, no-AES, and minimal ARM/RISC-V build wrappers passed;
  their three physical ARM cycles passed finite monitors. The unchanged
  upstream JCAlgTest CAP completed 4,525 current-profile rows and matched its
  generated support set (15 yes, 4,510 no). `/deploy` Core ran on all three;
  Security/Crypto ran on no-AES, and Security ran on minimal. Support-config
  test and whitespace check are recorded with this step. No new hardware test
  was run after detecting persistent-storage exhaustion.
- Result: Active ROM masks: 36,574 / 36,169 / 32,491 bytes; UF2 images:
  1,658,880 / 1,656,832 / 1,633,280 bytes (current / no-AES / minimal).
  Original JCAlgTest CAP SHA-256 remains
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
  No-AES upstream JCAlgTest startup returns `F103`; minimal JCAlgTest was
  skipped as directed. Current/no-AES Core has 17 OwnerPINx runtime exceptions
  and 2 wrong results; minimal reports 8 disabled-feature `ILLEGAL_USE`
  results. D92 memory probe found 23 persistent bytes free; later installs
  returned `6F00`, so those outcomes are storage-limited and not conclusive.
- Decision: Preserve the upstream CAP unchanged; use `/deploy` CAPs for reduced
  profiles. This physical acceptance matrix remains open: minimal Crypto and
  current Security/Crypto plus any remaining `/deploy` areas are unrun. Stop
  stateful physical installs on D92 and continue on a fresh card or emulator.
  No size measurement was made for the ROM-mask container beyond its recorded
  524,300-byte size; exact firmware hashes and logs are in the ignored profile
  artifact directory.
- Commit: not created; `git add` was blocked because `.git/index.lock` cannot be
  created on the read-only filesystem.
- Artifacts/logs: `artifacts/results/capability-profiles-acceptance-20261024/`,
  `artifacts/logs/profile-native-modules-20261024.log`,
  `artifacts/logs/test-javacard-support-20261024.log`, and deploy logs listed
  in `.agent/HANDOFF.md`.

## 2026-10-24 — Record selectable-profile acceptance commit

- Step/experiment: `selectable-profiles-acceptance-20261024-commit-record`
- Change: Recorded the commit SHA for the preceding selectable-profile
  acceptance step; its first commit was created after retrying Git writes with
  elevated host access.
- Validation: The preceding step's six support-config checks and whitespace
  validation passed. No additional build or hardware validation was run.
- Result: Acceptance documentation and related source/config test updates are
  committed; the original upstream JCAlgTest CAP remains unchanged.
- Decision: Continue the still-open `/deploy` acceptance matrix on a fresh
  physical card or VPCD; D92 has only 23 persistent bytes free.
- Commit: `36c8abf` (preceding step); this record is committed separately.
- Artifacts/logs: `artifacts/results/capability-profiles-acceptance-20261024/`.
# 2026-10-24 — Add symmetric cryptography profile

- Step/experiment: `symm-crypto-profile-implementation-20261024`
- Change: Added the `symm_crypto` profile based on the supplied NXP JCOP4 P71
  CSV; generated AES/DES cipher, transient key, and symmetric MAC providers;
  added Mbed TLS ECB paths and MAC primitives; enabled per-profile native
  module selection. The API submodule change is committed as `d0e7fc6`.
- Validation: `python3 tools/test-javacard-support.py` passed 7 tests;
  `./tools/build arm --profile symm_crypto`,
  `./tools/build riscv --profile symm_crypto`, and
  `./tools/build-pc --profile symm_crypto` passed; the bounded
  `./tools/test-pc-vpcd-applet --passes 1` smoke passed 15/15 APDU checks.
  Physical reader discovery via `sudo -n /usr/local/sbin/pico2-pcsc-reader`
  found 0 devices, so no flash or real-card test was attempted.
- Result: ARM UF2 is 1,670,656 bytes (SHA-256
  `86f203a09fffbbf795739b2b019baaf93f6a86219317d5803e521498ddbf1ca9`);
  PC ROM-mask image is 524,300 bytes (SHA-256
  `b8af1c28c384278a2442f5cd9e9e02c8cc1ef2d69b8977e9baaf14315d4d46e5`).
  Emulator checks establish applet selection, JCSystem info, SHA-256
  doFinal/static performance, and the existing AES CBC-NOPAD probe. They do
  not establish known-answer correctness for the newly added AES/DES modes or
  MAC algorithms; physical acceptance is still required.
- Decision: Keep `symm_crypto` as the target profile, but treat its new
  algorithms as build-validated only until cipher/MAC vectors and the complete
  `/deploy` Crypto area pass on a real card. Do not alter the upstream
  `AlgTest_v1.8.2_jc305.cap` (SHA-256
  `b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`).
- Commit: `2adca1c` (root implementation); `d0e7fc6` (Java Card API submodule).
- Artifacts/logs: `artifacts/logs/symm-crypto-vpcd-applet-final-20261024.log`;
  reference CSV SHA-256 `8fa3593c5f6a196e9ed5eb88a69bfac528c5acbcc933f5e4fdae36e3bf8d0943`.

## 2026-10-24 — Record symmetric crypto implementation commit

- Step/experiment: `symm-crypto-profile-commit-reference-20261024`
- Change: Recorded the implementation commit and normalized the supplied
  reference CSV file mode to non-executable without changing its contents.
- Validation: Confirmed the root commit is `2adca1c`, its API submodule pointer
  is `d0e7fc6`, and the CSV SHA-256 remains
  `8fa3593c5f6a196e9ed5eb88a69bfac528c5acbcc933f5e4fdae36e3bf8d0943`.
- Result: The implementation is committed; physical algorithm acceptance is
  still pending because no card reader was visible.
- Decision: Continue with real-card KATs and `/deploy` Crypto when a Pico reader
  becomes visible; do not report emulator results as hardware validation.
- Commit: `2adca1c` (implementation commit recorded here).
- Artifacts/logs: `artifacts/logs/symm-crypto-vpcd-applet-final-20261024.log`.
