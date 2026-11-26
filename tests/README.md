# Tests and validation fixtures

Use the stable `tools/test-*` wrappers from the repository root. Tests live in
`unit/`, `integration/`, and `hardware/`. Repository-owned applet sources
live in `fixtures/local/`. Put generated tests, CAPs and images under
`build/`, logs and results under `artifacts/`, and SDKs and converters under
`.local-toolchain/`. See [testing-data migration](../docs/testing-data-migration.md)
for original distributions and provenance.

## JCAlgTest inputs

- **Baseline for all new 3.0.5 runs.** The unchanged 1.8.5/jc305 CAP
  (`./tools/fixture-path jcalgtest.cap`, GETVERSION `1.8.5_jc305`,
  SHA-256 `fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`)
  and client `jcalgtest.client.1.8.5-20261121` (SHA-256
  `3cca70cbc243a773b998fa43792a135e26934d4bf616e7baa88c5369db18a703`).
  Pass this client explicitly for focused and full runs, including VPCD.
  The generic `jcalgtest.client` key still selects historical 1.8.4.
- **Supplemental 3.2 preview.** Use CAP key `jcalgtest.cap.1.8.6-jc32`
  and client key `jcalgtest.client.1.8.6`. See the
  [1.8.6 fixture notes](fixtures/external/jcalgtest/1.8.6/README.md).
  Run `JCALGTEST_VARIANT=jc32 ./tools/test-jcalgtest-support-vpcd`.
  Both CAPs share package and applet AIDs. Delete the installed variant
  before switching and restore 1.8.5/jc305 after board testing.
- **Historical inputs.** Preserve 1.8.2, 1.8.4, and earlier 1.8.5 results under
  their original identities. Existing campaigns may finish with their
  original client. The 1.8.4 client did not deliver INS_CARD_RESET because
  of a negative APDU length bug. The current baseline CAP fixes the RSA
  NOPAD setup input. For simulator comparisons, the 1.8.5-client reference
  used the 1.8.4/jc305 CAP and applies to support probing only.

Keep supplied CAP/JAR bytes unchanged. Run `./tools/fixture-path --verify`
before acceptance. The [manifest](fixtures/manifest.json) pins paths and hashes.
Record the CAP version and client hash separately. CSV version labels can be
wrong, and evidence without the client hash does not count for acceptance.

## Physical campaigns

Follow [AGENTS.md](../AGENTS.md) for host access, normal max_crypto and
empty-card acceptance. Use the exact reader and an explicit client override:

```bash
PICO2_HOST_USB_ACCESS=1 ./tools/run-jcalgtest \
  --client-jar "$(./tools/fixture-path jcalgtest.client.1.8.5-20261121)" \
  --reader-name '<exact identified PC/SC reader name>'
```

Prioritize option 2 **PERFORMANCE TEST** (all JC API methods, 256-byte data,
1–3 hours) and option 1 **All algorithms** (5–8 hours). Use different
identified tokens when possible. A run starts when the client connects to the
exact physical reader. Record reader name, chip ID, firmware hash, CAP version
and client hash. Keep per-token result directories and serial logs. Continue
on another available token after a stall or loss, with separate evidence.
Windows results are independent of physical Linux validation.

With object deletion enabled, runtime emits one
`OBJECT_DELETION:PRE_CLEANUP` line before each persistent reclamation pass.
It records the current raw file count and bytes/capacity for both applet-
requested and automatic requests. `tools/check-object-deletion-headroom.py`
aggregates the peak and fails if telemetry is absent, malformed, inconsistent,
or ever exceeds 90% of the arena. The VPCD JCAlgTest runner invokes this gate
when `JCALGTEST_OBJECT_DELETION=1` and saves its summary as `headroom.log`.

## Independent Java Card API suite

Use snapshot `provided-20261107`, including its own CAP variants and client.
Resolve `api.caps`, `api.client`, `api.runner`, and `api.gp` with
`./tools/fixture-path`. Use `tools/run-api-test --list` for the suite plan.
The JCAlgTest host client does not provide the API suite entry point used by
`tools/run-api-test` and `tools/run-deploy-parallel`.
