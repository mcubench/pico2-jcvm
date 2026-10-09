# Handoff

## Objective

Implement `plan.md` for CHOUPI on Pico 2, following root `AGENTS.md`. The user
requested software implementation without a physical device for now.

## Current state

- The root repository tracks only the plan and RP2350 references at the
  starting revision. `choupi/` and `AlgTest_JavaCard/` are user-provided,
  untracked source trees; preserve them and commit only files belonging to a
  completed step.
- The CHOUPI C++ VM source is present. `choupi/os/`,
  `choupi/javacard-sdk/javacard-api/`, and
  `choupi/javacard-sdk/rommask/` are empty dependency directories, despite
  their entries in `choupi/.gitmodules`.
- The AlgTest directory contains CAPs labeled v1.8.0 and v1.8.2; the plan's
  target artifact is v1.8.4. Do not silently treat these as equivalent.
- The environment currently has no `cmake`, `cargo`, `rustc`, Pico SDK, or
  ARM embedded compiler. A read-only `git ls-remote` attempt failed because
  `github.com` could not be resolved.
- `tools/doctor` now reports host build prerequisites, SDK presence, and
  dependency checkout status. It does not flash hardware.
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
  pass; target builds remain unavailable. Commit: pending.
- The requested `.agent/SESSION_CONTINUITY.md` was absent and has now been
  created. Root `progress.md` will be append-only.

## Next action

Continue WP5 by implementing VM-internal exception-to-Java-object mapping and
the Java API/APDU pieces. Then establish a host golden applet harness before
starting WP6. The provided `tools/build` wrapper is currently absent, and
calls for both architectures return status 127. When dependencies and build
tools are restored, populate the pinned submodules (`654f374`, `ac9f752`,
`d490faf`), verify the CHOUPI source revision, and establish the host baseline.

## Latest validation

- `bash -n tools/doctor`: passed.
- `./tools/doctor`: returned 1 and reported the missing tools, SDK variable,
  and dependency checkout paths listed in `progress.md`.
- Latest VM syntax check: `TEST:PASS vm_core_syntax`; existing upstream
  warnings remain.
- Latest `./tools/test-jcvmarray`: `TEST:PASS jcvmarray_fill`.
- `./tools/build arm` and `./tools/build riscv`: wrapper absent; both calls
  returned status 127.
- Latest interpreter exception/status step: `./tools/check-vm-syntax` passed
  with `TEST:PASS vm_core_syntax`; `./tools/test-jcvmarray` passed.
- Hardware validation: intentionally skipped; no device was requested yet.
