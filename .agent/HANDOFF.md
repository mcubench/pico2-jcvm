# Handoff — next steps only

Updated: 2026-11-15.

## External deletion test
- Current package: `build/external-test/pico2-jcvm-maxcrypto-deletion-d1130732.zip`.
- Current no-debug, normal-mode, empty-card ARM UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d1130732-current.uf2`.
- UF2 SHA-256: `80840af050b8a6e7f32c540a56b18421415143d4093c491bf9efcd41859d326c`; ZIP SHA-256: `5ef963a3bd654d449813d499e687c5040487c15a9251c10d1ce99261a6ab3ebe`.
- Request `WIN-OBJECT-DELETION-LOW-WATERMARK-D1130732` in `.agent/EXT_REQUESTS.md`; README has exact Windows flashing and JCAlgTest steps. Overlay tests, ARM build and package checks pass; no physical flash or Windows run yet. This candidate respects the 96 KiB INV-28 limit.

## Next actions and blockers
- Fresh-client VPCD T7 failed: option 2 reached sync 6,300, then persistent allocation returned `F205` at 98,246/98,304 bytes. Do not increase that limit without user approval; INV-28 fixes it at 96 KiB. See `TODO.md` and `artifacts/results/jcalgtest-support-vpcd-20261115T084412Z/`.
- Continue MEM-01 T10: diagnose why nearly all stored records are marked reachable; make reachability/reclamation more effective within INV-28, then prove live data survives and allocations continue in the no-deletion-request fake-flash/VPCD case.
- Await the external option-2 result for the exact UF2 hash above; this package is a test candidate, not acceptance evidence.
- Local reader prerequisite is unresolved: approved helper previously found four matching devices and refused; bounded reader listing timed out. See `HOST01-READER-COUNT-20261115` in `.agent/EXT_REQUESTS.md`; retry only after enumeration is restored.
- MEM-01 T7 and physical T8 remain open. Three captured option-2 replay cycles passed, but the fresh-client run failed and replay evidence is not client or hardware acceptance.
- Preserve all existing dirty source/test changes. The current image was built from committed HEAD `d1130732` plus unrelated pre-existing dirty files; inspect package `BUILD-PROVENANCE.txt` before attributing behavior to a clean checkout.
