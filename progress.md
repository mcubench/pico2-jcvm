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
- Commit: `pending`
- Artifacts/logs: no archived logs; test emitted the PASS records above
