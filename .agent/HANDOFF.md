# Handoff — next steps only

Updated: 2026-11-19 23:16 CET.

## Current state
- No campaign is live. 07E7 runs the Windows-package empty-card image `a266b836` (UF2 `63abbf3d…99f8b`), wiped then given the JCAlgTest 1.8.5 CAP and the EC384 replay objects.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- Open from this session: PERF-LAYOUT-01 (crypto timing depends on code placement; needs heap-peak data first).
- F42D (2026-11-18): empty-card external-test UF2 `a0b46b33…`; was not visible through PC/SC. T7 remains blocked on missing `Virtual PCD 00 00` (`HOST-VPCD-PCSC-01`); keep T7/T8/T10 open.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
