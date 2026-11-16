# Handoff — next steps only

Updated: 2026-11-16.

## Current state
- Source change pending commit: low-watermark tombstones now compact after any successful deletion batch, including applet-requested `JCSystem.requestObjectDeletion()` batches. Policy, RP2350 overlay, normal ARM build and focused VPCD tests pass.
- The previously delivered `b6ccf96c-dirty` external UF2 predates this fix. Rebuild and refresh the empty-card max-crypto Windows package after committing the fix.
- PC/SC helper granted access to three USB nodes, but elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` timed out after 20 s. Do not flash until an identified reader is enumerated.

## MEM-01 next actions
1. Commit the low-watermark compaction fix and evidence as one logical step.
2. Build a fresh normal ARM `max_crypto` empty-card UF2 with object deletion ON/debug OFF; refresh `TODO.md`, `EXT_REQUESTS.md`, provenance, and package hashes for Windows testing.
3. T7 VPCD option-2 is still incomplete. Post-fix campaign log: `artifacts/results/jcalgtest-support-vpcd-20261115T211711Z/`. It stopped at `TYPE_RSA_PRIVATE_CRT LENGTH_RSA_1536 setDP1()` without `JCALGTEST_VPCD:PASS`; do not count it as acceptance. Its overlay is valid (1,767 slots, 988 live, 45,517 bytes), but clean card/registry state was not saved, so do not resume from that folder without reconstructing and verifying the pair.
4. Post-fix partial metrics: 18,000 overlay syncs, all status 0; 31 successful low-watermark compactions, eight automatic triggers, maximum automatic-trigger use 79,308/98,304 bytes; no `F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, `FS_COMPACT_REJECT`, `FAULT`, deletion skips, or alias-protection events. Last 1,000-sync sample grew 33,457→45,525 bytes, so plateau gate remains open.
5. Refresh reader access before physical work; use the identified-board failover pool and run option 2 then option 1 on separate normal max-crypto/empty-card devices when available. Physical T8 remains open.
6. Continue T10: add pre-cleanup high-water/file-count acceptance telemetry and a >90% runner failure gate. Safe-point exhaustion reclaim/write retry remains unimplemented pending a proven safe replay point.

## External action
- `WIN-RSA1984-X50-SERIAL-20261115` remains open.
- Replace `WIN-MEM01-EMPTY-NODEBUG-B6CCF96C` with the fresh post-fix package after it is built.
