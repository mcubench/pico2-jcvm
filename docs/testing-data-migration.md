# External validation suites and test layout migration

Completed 7 November 2026 under explicit user authorization. No firmware
runtime, native API implementation, or external CAP/JAR/runner bytes changed.

## Final layout

- `tests/unit/`: native host tests and support-catalog tests.
- `tests/integration/`: emulator/APDU suites and shared Python helpers.
- `tests/hardware/`: physical host probes and PC/SC probe sources.
- `tests/fixtures/local/`: repository-owned applet fixture sources.
- `tests/fixtures/external/jcalgtest/1.8.4/`: supplied JC305 CAP and versioned
  client, preserving its adjacent `lib/` classpath.
- `tests/fixtures/external/javacard-api/provided-20261107/`: independent supplied
  API suite, all 48 CAPs across 221/222/303/304/305, its distinct client,
  libraries, runner and GP tool. Snapshot date identifies ingestion, not release.
- `tests/fixtures/external/jcalgtest/legacy/1.8.2/`: explicitly historical CAP
  and simulator reference; no active default selects these.
- `tests/fixtures/reference/`: newly generated 1.8.4 simulator reference,
  historical expected-support data and supplied card comparison CSVs.
- `tools/`: stable build/test/probe entrypoints; renamed Python tests retain
  compatibility entrypoints. The old versioned LOAD builder forwards to
  `tools/build-jcalgtest-load-code` and explicitly announces the new baseline.
- `build/`: extracted LOAD input, generated fixture CAPs, compiled tests and
  emulator images. Relocated CMake tests use fresh cache directories.
- `artifacts/`: results, logs and the original-distribution backup.
- `.local-toolchain/java-card/provided/`: supplied SDK/converter dependencies,
  provisioned locally and ignored. See `.agent/TOOLCHAIN.md`.

## Input provenance and preservation

`tests/fixtures/manifest.json` records 81 files with byte counts and SHA-256
hashes: 77 initially supplied suite/legacy assets, one fresh simulator reference
and three additional historical reference CSVs. No input was downloaded.
JCAlgTest 1.8.4 CAP SHA-256:
`cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.

The versioned JCAlgTest client and independent API client have different
hashes and are not interchangeable. The supplied versioned client identifies
itself internally as 1.8.3; its bundled simulator applet reports 1.8.4_jc305.
The new simulator reference was generated with that client on 7 November via
`tools/run-jcalgtest --simulator -op ALG_SUPPORT_EXTENDED -fresh`.
Simulator-specific unsupported/error rows are retained, not rewritten. This
reference does not establish Pico hardware acceptance.

The entire original tree, including old source, SDKs and historical result
files, is preserved at
`artifacts/backups/testing-data-migration-20261107/AlgTest_JavaCard/`.
All 388 original files passed before/after size and SHA-256 comparison.
The backup and toolchain are intentionally ignored; canonical suite fixtures
and the fixture manifest are versioned. Original-tree inventory and migration
logs are archived under `artifacts/results/testing-data-migration-20261107/`.
Historical review/progress/archive paths remain historical evidence; active
tool consumers no longer depend on the former root directory or tools/tests.

## Verification and limits

- Fixture verification passes all 81 records, including 48 unchanged API CAPs.
- Catalog integrity tests pass (2); Java Card support/comparison tests pass (14).
- Relocated CAP parser passes 4,208 checks / 100,000 fuzz inputs over 48 CAPs.
- JC305 method-body corpus check passes 11 CAPs / 978 concrete methods,
  with no clamps or invalid/misaligned branch targets.
- Native host wrapper batch: 26 passed, 2 failed. Initial CMake relocation
  cache mismatches were resolved with new build directories; remaining
  failures are listed below and accepted as documented follow-ups by the user.
- RMI fixture builder passes from its new source/toolchain locations.
- Python compilation (24 files), Bash syntax (107 entrypoints) and compilation
  of the relocated Java PC/SC probes pass.
- PC emulator build passes with existing upstream warnings.
- JCAlgTest 1.8.4 emulator/restart test passes 30 checks after rebuilding the
  emulator. An earlier run timed out; it is retained in the logs.
- API multi-package installation smoke passes: Core and Multi loaded and
  three applet instances installed. A separate extended-Crypto probe initially
  timed out before VPCD connection; the isolated retry passes 3 checks and
  returns the expected AES result `000020019000`.
- Firmware ARM/RISC-V builds, Windows firmware package production and physical
  USB/PCSC tests were intentionally skipped: no runtime source changed in this
  migration. No flash or physical acceptance claim is made.

Accepted host-test follow-ups (also in `TODO.md`):

1. `tools/test-jcvmarray-allocation`: after successful relocation/configuration,
   linking fails on the existing missing
   `FlashMemory_Handler::persistentWriteRollbackGeneration()` dependency.
   The current unrelated heap changes were preserved and not staged.
2. `tools/test-vpcd-protocol`: fails `send partial frame header`; investigate
   the existing timeout/socket lifecycle behavior without changing runtime
   semantics as part of this migration.
