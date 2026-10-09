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
- Commit: pending
- Artifacts/logs: `/tmp/vm-syntax.log` (temporary syntax-check output; no hardware log)
