# Handoff — next steps only

Updated: 2026-11-20 04:08 CET.

## Current state
- **No live run.** Option 1 on 07E7 (a266b836 image) ended 2026-11-20 04:00 CET with `*** PANIC *** / Out of memory` at RSA-3072 CRT NOPAD `doFinal` after 505 methods, the same point as the 2026-11-18 run; see progress `20261120-0408-option1-a266b836-oom` and TODO OOM T9. Option 2 on the same image passed (exit 0, 26 min). 07E7 re-enumerated on USB after the panic; its storage holds the campaign state (wipe before any acceptance run).
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- Next engineering item: heap OOM in option 1 (TODO OOM T9 (a)–(d)); do not rerun option 1 before a fix lands.
- F42D (2026-11-18): empty-card external-test UF2 `a0b46b33…`; was not visible through PC/SC. T7 remains blocked on missing `Virtual PCD 00 00` (`HOST-VPCD-PCSC-01`); keep T7/T8/T10 open.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
