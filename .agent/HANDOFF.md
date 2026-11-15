# Handoff — next steps only

Updated: 2026-11-15.

## External deletion test
- Current package: `build/external-test/pico2-jcvm-maxcrypto-deletion-afe8ed5d.zip`.
- Current no-debug, normal-mode, empty-card ARM UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-afe8ed5d-current.uf2`.
- UF2 SHA-256: `7d44ec93552fe1acad4824c0014109a47260064dd51df5935a272f169f607df0`; ZIP SHA-256: `f509456710e7527ca3545b763492c8e07e80bea446f5641800b6ddaacfc7f053`.
- Request `WIN-OBJECT-DELETION-LOW-WATERMARK-AFE8ED5D` in `.agent/EXT_REQUESTS.md`; package README has Windows flashing/JCAlgTest steps. ARM build, empty-card packaging, fixture verification, package checksums and ZIP integrity pass; no physical flash or Windows run yet. INV-28 remains 96 KiB / 8,192 slots.

## Next actions and blockers
- LONGOP-01 (P0 under HOST-01): every option-2 campaign dies at the first RSA-1984 private ×50 APDU. The confirmed defect is no CCID time extension during modexp; a RAM out-of-memory panic is suspected. Start with TODO LONGOP-01 T1 (offline heap measurement) and T2 (Linux physical, with the `__already_measured.list` resume trick). External serial evidence is requested as `WIN-RSA1984-X50-SERIAL-20261115`.
- Fresh-client VPCD T7 failed: option 2 reached sync 6,300, then persistent allocation returned `F205` at 98,246/98,304 bytes. Do not increase that limit without user approval; INV-28 fixes it at 96 KiB. See `TODO.md` and `artifacts/results/jcalgtest-support-vpcd-20261115T084412Z/`.
- Continue MEM-01 T10: diagnose why nearly all stored records are marked reachable; make reachability/reclamation more effective within INV-28, then prove live data survives and allocations continue in the no-deletion-request fake-flash/VPCD case.
- Await the external option-2 result for the exact UF2 hash above; this package is a test candidate, not acceptance evidence.
- Local reader prerequisite is unresolved: approved helper previously found four matching devices and refused; bounded reader listing timed out. See `HOST01-READER-COUNT-20261115` in `.agent/EXT_REQUESTS.md`; retry only after enumeration is restored.
- MEM-01 T7 and physical T8 remain open. Three captured option-2 replay cycles passed, but the fresh-client run failed and replay evidence is not client or hardware acceptance.
- Preserve all existing dirty source/test changes. The current image was built from committed HEAD `afe8ed5d` plus a dirty worktree; inspect package `BUILD-PROVENANCE.txt` before attributing behavior to a clean checkout.
