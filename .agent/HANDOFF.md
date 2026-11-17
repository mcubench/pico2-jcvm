# Handoff — next steps only

Updated: 2026-11-18 00:12 CET.

## Current state
- H1/H2/H3 are implemented through `de729d28`; current full JCVM ARM `max_crypto` image is normal (object deletion ON, debug OFF), SHA-256 `dabbdf0f9a8ba087314190ac54bc1606dfe81b7e051acaddf21acf46d77e7fa4`, source HEAD `00048e16`.
- Physical option 2 completed on fresh token `07E74C5D7A038DEC`: 4,660 s, 4,826 APDU responses all `9000`, 2,951 resource samples, peak 14.852% arena, 25 compactions, no FAULT/FAIL/PANIC. Client log explicitly says `Testing complete`; exit status was not captured. This is exploratory only until T7 passes. Actual per-attempt provenance is `artifacts/results/option2-acceptance-20261117T2113Z/07E74C5D7A038DEC/attempt5/campaign-identity.txt`; serial telemetry is in `attempt4/serial.log`.
- Physical option 1 attempt 1 on F42D was interrupted after about 7 minutes when the approved PC/SC helper refresh restarted `pcscd` during the active client session. Client log records `SCARD_F_COMM_ERROR` / `SCARD_E_NO_SERVICE` during DES cleanup, then the client exited with `NoSuchElementException`; this is a host transport failure, not a VM/memory failure. The separate serial capture remains available. Artifacts: `artifacts/results/option1-acceptance-20261117T2257Z/F42DD9D07D6D2B07/`. Do not restart PC/SC during a live campaign. Reader refresh after service recovery lists only 07E74C5D7A038DEC, so F42D is currently unavailable to PC/SC. Resume from its `already_measured.list` on the same token if it returns; otherwise record the switch and use an available token only after an empty-card reset.
- T7 VPCD option-2 attempt `artifacts/results/jcalgtest-support-vpcd-20261117T230259Z/` stopped before client startup: bounded reader discovery found only the physical Pico readers, not `Virtual PCD 00 00`. This host has no installed VPCD PC/SC driver/binary under `/usr/lib/pcsc` or PATH. External request `HOST-VPCD-PCSC-01` asks to restore that host service. Do not repeat the runner until the virtual reader is published; the failed attempt did not execute JCAlgTest.
- Preserve unrelated untracked `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and root `*_already_measured.list` files.

## Next actions
1. Wait for `HOST-VPCD-PCSC-01`; confirm `Virtual PCD 00 00` with `tools/list-readers`, then rerun the unchanged-client VPCD option-2 gate with the 1.8.5 JAR, deletion telemetry, and RP2350 overlay.
2. Recheck exact readers before hardware work. If F42D returns, resume option 1 from its saved progress list. If unavailable, use a different identified Pico 2 only after documenting the switch and resetting it to empty-card normal state. Do not run the helper during a live client campaign.
3. Update MEM-01 T7/T8/T10 and dependent RESOURCE-01/02 only from complete evidence. T8 remains exploratory until T7 passes.
4. Record and commit each completed logical step with its own progress entry and Step-Id. Do not stage unrelated untracked files.

Before any additional hardware operation, refresh the approved reader helper and exact-ID reader list. Never infer current board state from this handoff.
