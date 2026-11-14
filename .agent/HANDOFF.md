# Handoff — next steps only

Updated: 2026-11-14.

## Next actions
1. **HOST-01:** validate the new long-uptime CCID retry fix on hardware. The
   current D92 trace matches an unarmed zero deadline being interpreted as a
   future signed `time_us_32()` deadline after 35m 47s; explicit armed-state fix
   passes 35/35 unit checks and ARM build. PC/SC listing most recently timed
   out, so re-enumerate before testing and keep failure classes separate.
2. **MEM-01 T7:** complete bounded overlay-backed VPCD option-2 proof through
   sync sequence ≥2,500. Record live record/byte counts at compaction and alias
   roots. A previous post-fix run stopped near 800.
3. **MEM-01 T8:** use the newly prepared no-debug acceptance image and empty
   card for option-2/option-1 physical runs after T7/HOST-01. Preserve results
   separately per identified board.

## External test image ready
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-ccid-deadline-805420e6.uf2`
- SHA-256: `5fa91117aeb6b2ac1c6ebb537447090016b27f0717bd14e528cd5d9437e4a6de`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-nodebug-windows-ccid-deadline-805420e6.zip`
- Package SHA-256: `6a3e343eda7f88c6f845939cf94d71a5ca13d67dbc2b5d268bc1a64ccc0f8bf0`
- ARM max_crypto acceptance build; object deletion ON, all debug flags OFF;
  empty persistent storage; includes CCID retry fix. Build and package checks
  pass; hardware test pending.
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
