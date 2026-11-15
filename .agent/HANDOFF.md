# Handoff — next steps only

Updated: 2026-11-15.

## External deletion test
- Current package: `build/external-test/pico2-jcvm-maxcrypto-deletion-72cbeedc.zip`.
- Current no-debug, normal-mode, empty-card ARM UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-72cbeedc-current.uf2`.
- UF2 SHA-256: `2a6a71be9ff9cf4fcf98fedebf56dd41d74be91132ae169ad8d3d66998b1068c`; ZIP SHA-256: `32008fa05456b4dd927b5323e8d956beb2363d0e7f79708bc3e3f13928f6f1ad`.
- Request `WIN-OBJECT-DELETION-LOW-WATERMARK-72CBEEDC` in `.agent/EXT_REQUESTS.md`; README has exact Windows flashing and JCAlgTest steps. ARM build and package checks pass; no physical flash or Windows run yet.

## Next actions and blockers
- Fresh-client VPCD T7 failed: option 2 reached sync 6,300, then persistent allocation returned `F205` at 98,246/98,304 bytes. See the exact logs and measurements in `TODO.md` and `artifacts/results/jcalgtest-support-vpcd-20261115T084412Z/`.
- Implement MEM-01 T10 now: diagnose why nearly all stored records are marked reachable; add evidence-driven exhaustion headroom/reclaim only at safe points, then prove live data survives and allocations continue in the no-deletion-request fake-flash/VPCD case.
- Await the external option-2 result for the exact UF2 hash above; this package is a test candidate, not acceptance evidence.
- Local reader prerequisite is unresolved: approved helper previously found four matching devices and refused; bounded reader listing timed out. See `HOST01-READER-COUNT-20261115` in `.agent/EXT_REQUESTS.md`; retry only after enumeration is restored.
- MEM-01 T7 and physical T8 remain open. Three captured option-2 replay cycles passed, but the fresh-client run failed and replay evidence is not client or hardware acceptance.
- Preserve all existing dirty source/test changes. The current image was built from dirty HEAD `72cbeedc`; inspect package provenance before attributing behavior to a clean commit.
