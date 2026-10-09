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
- The requested `.agent/SESSION_CONTINUITY.md` was absent and has now been
  created. Root `progress.md` will be append-only.

## Next action

When the dependency sources and build environment are available, populate the
pinned submodules (`654f374`, `ac9f752`, `d490faf`), verify the CHOUPI source
revision, establish the host baseline, and begin WP1's Pico 2 CMake/startup
branch. Until then, continue with VM-only work that can be safely inspected
without the missing OS and SDK APIs; avoid claiming build validation.

## Latest validation

- `bash -n tools/doctor`: passed.
- `./tools/doctor`: returned 1 and reported the missing tools, SDK variable,
  and dependency checkout paths listed in `progress.md`.
- Hardware validation: intentionally skipped; no device was requested yet.
