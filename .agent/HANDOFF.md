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
