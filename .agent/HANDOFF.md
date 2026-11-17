# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- QKB.md was reread: “no boards” meant no attached board was running JCVM, not no hardware. Approved helper + `./tools/list-readers --pico-only` identified F42DD9D07D6D2B07 as a JCVM reader.
- LONGOP-01 T3 cooperative CCID progress is implemented and the focused physical RSA-1984 path passes.
- Commit `edb59d2b` records a direct physical APDU matrix on F42D with normal `max_crypto` firmware SHA-256 `61c2d449ec5402b88c5735237eae076ae6a933820897e38371e9f5c7f4afa4f7`: RSA-1984 PKCS1 ×50 (48.775 s, 47/47 extensions, min available memory 70,120 B); RSA-1536 ISO14888 ×50 control (1.721 s, no extension); RSA-2048 PKCS1 ×50 (49.174 s, 47/47 extensions, min 69,488 B); RSA-2048 SHA-PKCS1 sign (`AA9000`, 2.278 s). All APDUs returned `AA9000`; finite serial capture had no fault/reboot. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T170835Z/F42DD9D07D6D2B07/`.
- T6 remains open: direct APDUs are not unchanged-client resume-list acceptance or full option-2. The prior 1.8.5 resume-list attempt repeated RSA-1984 ×50 and timed out at 300 s before reaching RSA-2048; log: `artifacts/results/longop01-rsa1984-physical-20261117/F42DD9D07D6D2B07/ALGTEST_log_1794932230136.log`.
- T5 source audit found E4 DELETE has no `NativeOperationScope` and its persistent commit synchronously erases/programs a full filesystem snapshot inside `flash_safe_execute` (5 s timeout). The main-loop 15 s watchdog refresh is after `choupi_ccid_task()` returns. This is not yet a physical reproduction or measured DELETE duration.
- T1/D2 remain open: 250 ms free-memory samples miss brief peaks and largest contiguous block. T2, T5 reproduction, T6, MEM-01 T7/T8 remain open.

## Next actions
1. Refresh approved host access and reader identity; reproduce JCAlgTest DELETE on the normal image with bounded serial capture. Record APDU status, elapsed time, extensions, filesystem size and watchdog/fault evidence; reinstall unchanged CAP afterward if deleted.
2. Continue target peak/fragmentation measurements for RSA, then pursue normal-image empty-card option-2 acceptance when available.
3. Do not repeat the same broad resume-list from its prior start: it replays repeated RSA-1984 rows before RSA-2048. Build a narrowly filtered scratch resume list only if it preserves client code paths and targets the missing path.
