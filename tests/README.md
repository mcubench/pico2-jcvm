# Tests and validation fixtures

Run tests through the stable `tools/test-*` wrappers from the repository root.
Test implementations live in `unit/`, `integration/` and `hardware/`.
Repository-owned applet/fixture sources live in `fixtures/local/`.

`fixtures/external/` contains immutable, supplied validation assets:

- JCAlgTest **1.8.4**, Java Card Classic 3.0.5, and its versioned client.
- The independent Java Card API validation suite, snapshot
  `provided-20261107`, including all 48 CAPs and its own client/runner.

`fixtures/manifest.json` selects the baseline and records original paths,
versions, byte counts and SHA-256 hashes. Resolve assets with
`./tools/fixture-path jcalgtest.cap` (or `jcalgtest.client`, `api.caps`,
`api.client`, `api.runner`, `api.gp`). Verify the complete supplied set with
`./tools/fixture-path --verify`. Python tests share `fixtures/paths.py`.
Run `tools/run-api-test --list` to inspect the supplied API suite's plan.

Keep input CAP/JAR bytes unchanged. Extracted LOAD components, compiled tests,
generated test CAPs and card images belong under ignored `build/` directories.
Logs, measurements and result CSVs belong under `artifacts/`; old results keep
their original applet version and do not establish 1.8.4 acceptance.
Legacy 1.8.2 assets/reference CSVs are explicitly under
`fixtures/external/jcalgtest/legacy/1.8.2/`, never selected by default.

Supplied converters/SDKs are toolchain dependencies, provisioned at
`.local-toolchain/java-card/provided/` in this workspace. They are not test
fixtures. The original provided tree is archived during migration so no
source, older artifact, or result is discarded. See
`docs/testing-data-migration.md` for the inventory and migration evidence.

Simulator/card comparison input lives under `fixtures/reference/`. It retains
its recorded applet version and provenance. The supplied versioned client
reports an internal version of 1.8.3; its simulator applet is 1.8.4_jc305.
The original distribution is preserved at
`artifacts/backups/testing-data-migration-20261107/AlgTest_JavaCard/`.
See `docs/testing-data-migration.md` for checks and accepted follow-ups.
