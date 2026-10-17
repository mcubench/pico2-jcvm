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
- Commit: pending metadata commit.
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
- Commit: pending.
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
- Commit: pending.
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
