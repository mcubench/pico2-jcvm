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
