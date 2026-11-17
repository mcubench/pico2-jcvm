# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- QKB.md was reread: “no boards” meant no attached board was running JCVM, not no hardware. Approved helper + `./tools/list-readers --pico-only` currently identify F42DD9D07D6D2B07 as a JCVM reader.
- LONGOP-01 T3 cooperative CCID progress is implemented and its focused physical RSA-1984 path passes.
- A direct physical APDU matrix passed on F42D with normal `max_crypto` firmware SHA-256 `61c2d449ec5402b88c5735237eae076ae6a933820897e38371e9f5c7f4afa4f7`: RSA-1984 PKCS1 ×50 (48.775 s, 47/47 extensions, min available memory 70,120 B); RSA-1536 ISO14888 ×50 control (1.721 s, 0 extensions); RSA-2048 PKCS1 ×50 (49.174 s, 47/47 extensions, min 69,488 B); RSA-2048 SHA-PKCS1 sign (`AA9000`, 2.278 s). All APDUs returned `AA9000`, and serial contained no fault/reboot. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T170835Z/F42DD9D07D6D2B07/`.
- This direct probe is not the unchanged-client resume-list acceptance or full option-2 campaign. LONGOP-01 T1/D2, T2, T5, T6 and MEM-01 T7/T8 remain open. Periodic free-memory samples do not measure sub-250 ms peaks or largest contiguous block.

## Next actions
1. Reproduce the resume-list RSA-1984/RSA-2048 Cipher and Signature paths with the pinned JCAlgTest 1.8.5 client; preserve pristine list, per-run logs and serial telemetry. Direct APDU matrix is supporting evidence only.
2. Audit flash-heavy DELETE paths for watchdog and CCID progress service.
3. Continue target peak/fragmentation investigation, then pursue normal-image empty-card option-2 acceptance when the board is available.
