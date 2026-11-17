# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Case 89 is fixed: non-atomic persistent array copies use the bulk range API, and large Rust filesystem replacements stage through the reserved defrag sector. The VPCD overlay uses the same range semantics.
- Case 89 passed: `01 5A 9000`; filesystem recovery test passed across four simulated write interruption points.
- ARM `max_crypto` normal build passes, object deletion ON, debug OFF, with 36,000 bytes remaining. No board was available for a full JCVM cycle.
- MEM-01 T7 long unchanged-client run and T8 physical campaigns remain open. Refresh reader access before retrying.
- Full OS host suite aborted in its MPU access test in this environment. Filesystem group passed (37 passed, 2 ignored).

## Next actions
1. When a Pico 2 is enumerated, run a full `./tools/cycle choupi --profile max_crypto` and inspect finite serial output.
2. Resume the unchanged-client option 2 VPCD run and physical option 2/1 campaigns when PC/SC access is available.
3. Continue MEM-01 T7 telemetry and T8 acceptance; use the 1.8.5 client and record its JAR hash.
4. Keep `.agent/EXT_REQUESTS.md` and `.agent/QKB.md` current as external blockers or robust environment fixes change.
