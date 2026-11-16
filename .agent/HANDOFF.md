# Handoff — next steps only

Updated: 2026-11-16.

## Current state
- Low-watermark deletion compaction is committed as `98fcb151` (`20261116-0800-mem01-lowwatermark-compaction`).
- Added pre-cleanup file/byte/capacity records for each physical and PC/VPCD object-deletion batch, plus a strict >90% headroom gate for VPCD and Windows serial capture. ARM and PC overlay builds pass; identified board `8E464F4E2BBC0A19` passed the full JCVM cycle.
- Focused RP2350-overlay VPCD auto-GC check passed 515/515 APDUs. The checker passed 44 samples with peak 78,027/98,304 bytes (79.37%). Five telemetry parser/policy unit tests pass.
- Unchanged 1.8.5 option-2 VPCD run with object deletion ON is currently running: `artifacts/results/jcalgtest-support-vpcd-20261116T071855Z/`. Latest observed peak at 1,647 samples: 42,006/98,304 bytes (42.73%), no failure marker. Do not start another concurrent campaign; monitor this exact run to its pass/fail/timeout and record final metrics. Its client and emulator logs are growing.
- The external empty-card package `98fcb151` predates pre-cleanup telemetry. Build and package a fresh normal max-crypto empty-card UF2 after committing the telemetry changes, then replace the current external request.
- Physical pool was identified as `8E464F4E2BBC0A19`, `2BED26ED20556535`, and `D92E743D5F640C0F`; one has just been used for the successful source-validation cycle. Refresh access before more hardware tests.

## MEM-01 next actions
1. Finish the running VPCD option-2 run; require no `F205`/`NO_RESOURCE`/storage/fault errors, telemetry ≤90%, and `JCALGTEST_VPCD:PASS`.
2. Build and package a fresh external normal max-crypto empty-card image including telemetry; preserve the upstream CAP and 1.8.5 client hashes.
3. Run physical option 2 then option 1 on separate identified normal max-crypto empty-card tokens when possible; capture serial logs and enforce the new headroom gate.
4. Continue T10 safe-point replay/retry design and garbage-allocation tests without applet deletion requests; address remaining HOST-01/LONGOP-01 stalls and overlay plateau evidence.
