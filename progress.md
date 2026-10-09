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
