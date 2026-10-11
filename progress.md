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
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T115001Z.log` (SHA-256 `e01b623fc94299571e1f72fde115acf6b0a7e3bbbe853f6ae9bc9954775c2903`); `artifacts/logs/cycle-choupi-20261010T115247Z.log` (identical to prior silent capture, SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`)

## 2026-10-10 — Add Pico heap high-water diagnostics

- Step/experiment: `wp1-choupi-pico-heap-highwater-diagnostic`
- Change: Enabled `PICO_DEBUG_MALLOC` on the Pico SDK allocator used by the ARM CHOUPI target with a `0x20070000` low-water threshold, so failed requests and allocations near the stack limit are reported without logging normal low-address allocations.
- Validation: `./tools/build arm` and `./tools/build riscv` both passed after correcting the SDK interface-target compile-definition scope. `./tools/env ninja -C build/arm -t commands` confirmed the allocator compile command carries `PICO_DEBUG_MALLOC=1` and the threshold. No hardware flash was attempted because the Pico had left BOOTSEL; the stage-marker image is also awaiting the user-requested physical BOOTSEL reconnect. UF2 SHA-256: `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`.
- Result: A diagnostic image is ready, but the failing allocation and startup phase remain unmeasured until this image is flashed and captured.
- Decision: Verify the board in BOOTSEL after the pending physical reconnect, then flash and inspect finite serial output.
- Commit: pending
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `19532c092fa19bcc7dd1e43e8d5aa84450755e112a02225ec12b697836f3cc9b`; build logs not archived

## 2026-10-10 — Capture CHOUPI heap exhaustion on Pico 2

- Step/experiment: `wp1-choupi-pico-heap-panic-localization`
- Change: Flashed the heap high-water and runtime-stage diagnostic image on the physically reconnected Pico 2 and captured its finite startup output.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, showed 100% ARM UF2 flash progress, and recorded an 18-second board capture. `fs_init()` reported done; `runtime()` started; the cycle correctly failed its runtime acceptance gate on Pico SDK `*** PANIC *** / Out of memory`.
- Result: One 36,870-byte allocation succeeded at `0x20068FB0..0x20071FB6`; the next 36,870-byte allocation failed. Exact allocating call site remains unknown. Log SHA-256: `9846ef5043fbd4b74f828a4a4da3260b538933db5455d9bf8027c56c7e51a9fb`.
- Decision: Free more static SRAM, then repeat both build gates and the real board cycle. Runtime and CAP installation are not passing yet.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T133620Z.log`

## 2026-10-10 — Test 64 KiB CHOUPI overlay arena on Pico 2

- Step/experiment: `wp1-choupi-overlay-arena-64k-hardware`
- Change: Reduced the overlay byte arena from 128 KiB to 64 KiB to free 65,536 bytes of SRAM for runtime allocations.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, flashed the RP2350 ARM UF2 with visible 100% progress, and captured 18 seconds of serial output. The runtime acceptance gate failed on `*** PANIC *** / Out of memory`.
- Result: After `runtime:start`, a 16,390-byte allocation succeeded at `0x2006EFE0..0x20072FE6`, but a subsequent 36,870-byte allocation failed. The reduced arena alone did not let the VM start. Log SHA-256: `4ea1a3f80e2bb24e5665d4e3e3e178e76fa6cc858a3e20fd5d3d31acee11761c`; UF2 SHA-256: `a590f7b12e9a9ebc66ffa202921e9878cc12b8457d667ed65f29aba97046c9f5`.
- Decision: Free further static SRAM while retaining a smaller overlay, rebuild both architectures, and repeat the real board cycle. The Pico must be physically returned to BOOTSEL. CAP remains uninstalled.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T212007Z.log`

## 2026-10-10 — Reduce CHOUPI overlay SRAM footprint

- Step/experiment: `wp1-choupi-overlay-arena-32k-compact-index`
- Change: Reduced the overlay byte arena to 32 KiB and compacted each overlay index entry from 12 to 8 bytes by using 16-bit offsets and sizes; the arena bound ensures both fields fit while retaining up to 8,192 indexed files.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. The built ARM CHOUPI UF2 SHA-256 is `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`. No runtime test has been recorded yet: USB shows the Pico in serial application mode, and `picotool reboot -f -u` did not re-enumerate it in BOOTSEL.
- Result: Both variants compile; static SRAM usage is reduced by about 96 KiB relative to the previous 64 KiB arena/index configuration. The VM runtime behavior and capacity impact have not been measured.
- Decision: Flash and inspect a finite serial capture after the Pico is confirmed in BOOTSEL. Do not treat this candidate as a runtime pass.
- Commit: pending
- Artifacts/logs: `build/arm/pico2_choupi.uf2` SHA-256 `46f2b453d33ada46fa1cda9e681f18648f53e00ac966b76bc9aa1faf49bda137`

## 2026-10-10 — Diagnose silent CHOUPI upload attempt

- Step/experiment: `wp1-choupi-32k-serial-capture`
- Change: Attempted the repository-gated CHOUPI flash cycle on the connected Pico 2, then reset once with the serial reader already attached and preserved both bounded captures.
- Validation: `./tools/cycle choupi` rebuilt ARM and RISC-V successfully, attempted `picotool load -f -x`, and failed the runtime output gate because no serial bytes arrived. `picotool reboot -f -a` also completed, but the 20-second pre-attached serial capture was silent. `picotool info -a` continues to identify the device as running USB serial firmware, not BOOTSEL.
- Result: The candidate was not confirmed running; neither capture included BOOT or TEST output. No runtime or flash success is claimed. The cycle log is byte-identical to a previous silent capture (SHA-256 `9bbe51071dbc1639f08ec2cac872821a9c3aa6dbad96e05ec04360da9df34b3c`); the pre-attached reset capture SHA-256 is `15125998cb93fe0190829ac3a6ffec0bfe84b365bc903605d437a42f39c98c65`.
- Decision: Have the board physically enter BOOTSEL, confirm it with `picotool info -a`, then retry the cycle and check for visible flash progress and startup output.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T212553Z.log`; `artifacts/logs/serial-reset-capture-20261010.log`

## 2026-10-10 — Verify software BOOTSEL recovery path

- Step/experiment: `wp1-choupi-force-bootsel-recovery`
- Change: Retried the documented `picotool reboot -F -u` transition with direct USB access after the silent upload attempt.
- Validation: The command returned success and reported that the device was asked to reboot into BOOTSEL; the following `picotool info -a` still found only USB serial mode and exited 249.
- Result: Software recovery does not leave this board accessible in BOOTSEL. No firmware write occurred during this experiment.
- Decision: Physically hold BOOTSEL while reconnecting, then confirm `boot type: bootsel` before flashing.
- Commit: pending
- Artifacts/logs: none

## 2026-10-10 — Boot CHOUPI after overlay SRAM reduction

- Step/experiment: `wp1-choupi-overlay-32k-hardware`
- Change: Flashed the committed 32 KiB overlay arena and compact 8-byte index on the RP2350 Pico 2 after confirming BOOTSEL.
- Validation: `./tools/cycle choupi` passed ARM and RISC-V build gates, visibly flashed 100%, and captured the 18-second serial stream. The cycle failed the runtime acceptance gate on an uncaught VM exception.
- Result: `fs_init()` completed and the former heap exhaustion is gone. A 36,870-byte allocation succeeded at `0x2006C000..0x20075006`, then CHOUPI reported `VM_EXCEPTION id=5 opcode=0x87 pc=0x10043C01` (IOException from `putfield_a`). The image-relative PC maps into ROM-mask record tag `0114`, at data offset `0x1B1`. Log SHA-256: `d2b068c660382e1b828004aea92132bd3f8f0cdccac0086a17369fd3c63d5d2f`.
- Decision: Keep the SRAM reduction; instrument filesystem write rejection causes and retry the hardware cycle to identify why persistence throws IOException.
- Commit: pending
- Artifacts/logs: `artifacts/logs/cycle-choupi-20261010T213245Z.log`

## 2026-10-10 — Add filesystem write rejection diagnostics

- Step/experiment: `wp1-choupi-fs-write-rejection-diagnostic`
- Change: Added explicit reasons and requested/existing ranges when `write_at()` rejects a missing file or out-of-range write; also report full-index and overlay-capacity rejection. Narrowed the new file offset assignment to the 16-bit overlay representation.
- Validation: `./tools/build arm` and `./tools/build riscv` passed. `./tools/cycle choupi` passed both build gates but could not transition the running CHOUPI image to BOOTSEL; it recorded no flash progress or serial output and failed the runtime gate. Diagnostic UF2 SHA-256: `1de6bce7ed146f0a4a709f3180650305c6d62eb2a902d0eb95d774dd49b88e13`.
- Result: The diagnostic image has not been flashed. The current app remains running and software force-reboot did not expose BOOTSEL.
- Decision: Re-enter BOOTSEL with the physical button, verify, then repeat `./tools/cycle choupi` to capture the write rejection.
- Commit: pending
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
- Commit: pending
- Artifacts/logs: no monitor file was produced; latest cycle log remains `artifacts/logs/cycle-choupi-20261011T054755Z.log`
