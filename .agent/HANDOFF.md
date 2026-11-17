# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Case 89 is fixed and passed VPCD: `01 5A 9000`; Rust filesystem recovery passed four interruption points.
- Post-change 1,590-APDU transcript replay passed with object deletion enabled. This was transcript replay, not a fresh client campaign.
- RP2350 overlay filesystem harness passed; T7 synthetic stress/recovery coverage is complete. T7 unchanged-client option 2 and T8 physical campaigns remain open.
- RSA-1984 private PKCS1 ×50 APDU passed a focused fresh-install VPCD replay (`B031` prepare, `B043` measure). This is not allocator or Pico RAM evidence; LONGOP-01 T1 remains open.
- ARM `max_crypto` normal build previously passed with object deletion ON and debug OFF. No Pico 2 was available for a full JCVM cycle.
- PC/SC reader enumeration most recently returned `list() failed`; host service tools are unavailable in this restricted environment.

## Next actions
1. For LONGOP-01 T1, add a bounded allocator/heap probe that records C-heap peak and JC live arrays/handles around RSA-1984 ×50; also run RSA-1536 control. Keep D2 unconfirmed until this is measured.
2. When a Pico 2 is enumerated, run a full `./tools/cycle choupi --profile max_crypto` and inspect finite serial output; then pursue identified-board physical acceptance.
3. Resume T7 unchanged-client option 2 and T8 physical option 2/1 runs when PC/SC access is available. Use the pinned 1.8.5 client and record its JAR hash.
4. Keep `.agent/EXT_REQUESTS.md` and `.agent/QKB.md` current when blockers or external requests change.
