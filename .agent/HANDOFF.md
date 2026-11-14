# Handoff — next steps only

Updated: 2026-11-14.

## Next actions
1. **MEM-01 T7:** complete bounded overlay-backed VPCD option-2 proof through
   sync sequence ≥2,500. Record live record/byte counts at compaction and alias
   roots. A previous post-fix run stopped near 800.
2. **HOST-01:** inspect why D92's final CCID response stayed queued with an idle
   IN endpoint (`pending=1`, `in_busy=0`, no submit errors/completion) and why
   wait/recovery telemetry was absent. Do not start another long physical run
   until the transition is understood or instrumented.
3. **MEM-01 T8:** use the newly prepared no-debug acceptance image and empty
   card for option-2/option-1 physical runs after T7/HOST-01. Preserve results
   separately per identified board.

## External test image ready
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-ba7e4aa1.uf2`
- SHA-256: `88e620964a0a52a6d061f2a3c14f538227f11ba0962d6f32972d6bc534545b6e`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-nodebug-windows-ba7e4aa1.zip`
- Package SHA-256: `ea11fb0fd6274af68c9f7840c4ba2dc13dcae5a8eb2b9ea79997028c193ddeec`
- ARM max_crypto acceptance build; object deletion ON, all debug flags OFF;
  empty persistent storage. Build and package checks passed; no hardware test.
- User reports two boards reinserted in BOOTSEL. On 2026-11-14 the approved
  helper granted three USB nodes, but `tools/list-readers` timed out after 20 s
  with no inventory. No token was reset or flashed. Re-enumerate before any
  hardware operation; node ACL access is not proof of a PC/SC reader.
- `./tools/test-rp2350-fs-overlay` passed `TEST:SUMMARY fail=0` in this session.
  The VPCD JCAlgTest run was not started because bounded PC/SC enumeration hung.

## D92 exploratory run
- D92 `D92E743D5F640C0F`: diagnostic image SHA-256
  `079ae6883c60b2fe12013c3e84bac0f5a25f35554f1215ddb96304d67c01225a`; option 2
  stopped at AES-192 with `SCARD_E_NOT_TRANSACTED` after 121 method groups and
  289 rows. No storage exhaustion/fault indicators; final CCID response stayed
  pending. See `.agent/EXT_REQUESTS.md` and `progress.md`.
