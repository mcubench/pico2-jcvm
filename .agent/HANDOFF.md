# Handoff — next steps only

Updated: 2026-11-15.

## External deletion test
- Current package: `build/external-test/pico2-jcvm-maxcrypto-deletion-e9c4deb3.zip`.
- Current no-debug, normal-mode, empty-card ARM UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-e9c4deb3-current.uf2`.
- UF2 SHA-256: `8615f92c94a902e04af14f9690fc7032609ae61fc476ad4526c2b949d0cba273`; ZIP SHA-256: `f9ef8641dd7bbd888aa0c1c32f822f4baa4489653cc491114eafd2fd9f4d74bf`.
- Request `WIN-OBJECT-DELETION-LOW-WATERMARK-E9C4DEB3` in `.agent/EXT_REQUESTS.md`; package README has exact Windows steps. ARM build and packaging pass; no physical or Windows test yet.

## Next actions and blockers
- Await the external option-2 result and serial log for the exact UF2 hash above.
- Local reader prerequisite is unresolved: approved helper previously found four matching devices and refused; bounded reader listing timed out. See `HOST01-READER-COUNT-20261115` in `.agent/EXT_REQUESTS.md`; retry only after enumeration is restored.
- MEM-01 fresh-client T7 and physical T8 remain open. Three captured option-2 replay cycles passed, but are not fresh-client or hardware acceptance.
- Preserve all existing dirty source/test changes. The current image was built from dirty HEAD `e9c4deb3`; inspect package provenance before attributing behavior to a clean commit.
