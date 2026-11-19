# Handoff — next steps only

Updated: 2026-11-19 22:59 CET.

## Current state
- No campaign is live. 07E7 runs normal `max_crypto` from `c44967d9` (FLASH-01b/e/f journal, FLASH-01d deletion skip, FLASH-03 OwnerPIN), UF2 SHA-256 `1282101da6c95cd1e99e01f6c079b73a0e2bbf5a54b1387649a57599e6ec5fa8`. Card holds the JCAlgTest 1.8.5 CAP and the `JCTransactionProbe` CAP (installed with `--params 7f`); it is not an empty card.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- `.agent/EXT_REQUESTS.md` `WIN-MAXCRYPTO-BASELINE-5A537513` packages firmware older than FLASH-01/03; rebuild the Windows package from tip if the user wants the external run to cover them.
- Open from this session: PERF-LAYOUT-01 (crypto timing depends on code placement), RV-20261106-04 cold power-cycle and persistent OwnerPINx cases.
- F42D (2026-11-18): empty-card external-test UF2 `a0b46b33…`; was not visible through PC/SC. T7 remains blocked on missing `Virtual PCD 00 00` (`HOST-VPCD-PCSC-01`); keep T7/T8/T10 open.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
