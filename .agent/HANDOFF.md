# Handoff — next steps only

Updated: 2026-11-15.

## External deletion test
- Current package: `build/external-test/pico2-jcvm-maxcrypto-deletion-d73e3411.zip`.
- Current no-debug, normal-mode, empty-card ARM UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d73e3411-current.uf2`.
- UF2 SHA-256: `14fce03c058c92f76b6677134412aab8c4ca96718d0607a79612be51003b4138`; ZIP SHA-256: `d7fe6bbeda10357a61b79cfb44a9ba0e2fb80146a6f9264bceb7a80438f3ed07`.
- Request `WIN-OBJECT-DELETION-LOW-WATERMARK-D73E3411` in `.agent/EXT_REQUESTS.md`; README has exact Windows flashing and JCAlgTest steps. Overlay tests, ARM build and package checks pass; no physical flash or Windows run yet.

## Next actions and blockers
- Fresh-client VPCD T7 failed: option 2 reached sync 6,300, then persistent allocation returned `F205` at 98,246/98,304 bytes. The current image increases the filesystem payload ceiling from 96 KiB to 112 KiB; this is not yet validated against that client case. See `TODO.md` and `artifacts/results/jcalgtest-support-vpcd-20261115T084412Z/`.
- Implement MEM-01 T10 now: diagnose why nearly all stored records are marked reachable; add evidence-driven exhaustion headroom/reclaim only at safe points, then prove live data survives and allocations continue in the no-deletion-request fake-flash/VPCD case.
- Await the external option-2 result for the exact UF2 hash above; this package is a test candidate, not acceptance evidence.
- Local reader prerequisite is unresolved: approved helper previously found four matching devices and refused; bounded reader listing timed out. See `HOST01-READER-COUNT-20261115` in `.agent/EXT_REQUESTS.md`; retry only after enumeration is restored.
- MEM-01 T7 and physical T8 remain open. Three captured option-2 replay cycles passed, but the fresh-client run failed and replay evidence is not client or hardware acceptance.
- Preserve all existing dirty source/test changes. The current image was built from committed HEAD `d73e3411` plus unrelated pre-existing dirty files; inspect package `BUILD-PROVENANCE.txt` before attributing behavior to a clean checkout.
