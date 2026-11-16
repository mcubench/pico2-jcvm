# Handoff — next steps only

Updated: 2026-11-16.

## Current state
- Low-watermark deletion compaction is committed as `98fcb151`; pre-cleanup telemetry and the 90% gate are committed as `1a28fa98` (`20261116-0825-mem01-precleanup-headroom`).
- Normal ARM `max_crypto` build passed, and identified board `8E464F4E2BBC0A19` passed the full JCVM cycle. Focused RP2350-overlay auto-GC passed 515/515 APDUs; headroom checker passed 44 samples at 78,027/98,304 bytes (79.37%). Five telemetry policy/parser tests pass.
- The 1.8.5 option-2 VPCD run was intentionally interrupted at the user's request, without a completion marker. It reached RSA private 1536 `ALG_RSA_ISO14888 Cipher_doFinal()`. SIGINT caused the host emulator's ptrace shim to abort (`Unknown ptrace signal received: 2`); treat the client `Card removed` error as shutdown fallout, not as card-side behavior.
- Partial run: `artifacts/results/jcalgtest-support-vpcd-20261116T071855Z/`. Headroom passed 1,935 samples, peak 55,414/98,304 (56.37%). Overlay inspection passed: 4,544 slots, 663 live records, 55,142 bytes used, 43,162 available. No `F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, or `FAULT` was logged. `empty-card.bin.cap-registry` is missing, so the run is not resumable as a client state.
- Current external package `98fcb151` predates the telemetry additions. Create a fresh normal max-crypto empty-card package after resuming work.

## MEM-01 next actions
1. Restart unchanged-client option 2 from fresh VPCD card/registry/overlay state and let it reach `JCALGTEST_VPCD:PASS` or its configured timeout; avoid Ctrl-C because no graceful mid-suite shutdown path is verified.
2. Build/package a fresh normal max-crypto empty-card external image including telemetry and update the request/status table.
3. Run physical option 2 and option 1 on separate identified empty-card tokens when possible; enforce the telemetry gate and record per-device hashes/logs.
4. Continue T10 safe-point exhaustion replay/retry design and garbage-allocation tests without applet requests, plus HOST-01/LONGOP-01 and plateau evidence.
