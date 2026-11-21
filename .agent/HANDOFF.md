# Handoff — next steps only

Updated: 2026-11-21 15:03 CET.

## Current state
- **No live campaign** (2026-11-21 12:26). MEM-01 T8 option 1 PASSED on 07E7 (9228c583, 1,901 methods over two segments, `artifacts/results/t8-option1-9228c583-20261120T2006Z/07E74C5D7A038DEC/`). 07E7 still holds that card state; reader `... (07E74C5D7A038DEC) 00 00`.
- **F42D in use (claude-opus-5-5, from 2026-11-21 15:10 CET):** short JCAlgTest check of the HMACKey / AES-MAC fixes (c42fab06) on its a9ec9e04 image with client 3cca70cb, then a diagnostic recoverable-reclaim fault-injection image. Do not use F42D until this note is removed.
- Firmware at HEAD (CAPIN-05/07 changes after 9228c583) has not been flashed; the live campaign uses 9228c583.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- 07E7 is in use for focused native crypto development validation. Storage-preserving cycle on the DRBG tree passed (UF2 SHA-256 `7f6ea308a86888580717523cc89faf8070da00fcb1bf66a09274a5bde67706ad`). Use the exact reader `Pico 2 CHOUPI CCID [JCVM CCID] (07E74C5D7A038DEC) 01 00`. F42D's RSA replay remains live, so avoid any shared PC/SC refresh.
- Native SHA3, DES MAC4, CFB/XTS, GCM/CCM, CRC16/32, pseudo-random and preseeded DRBG commits landed. The max_crypto simulator suite passed 206 vectors. Physical crypto KAT is still in progress.
- On-card RSA 3072/4096 development runs exercise two keypair types per modulus. See `artifacts/logs/test-rsa3072-hardware.log`. The four-byte physical preflight APDUs initially failed with 6F00. Supplying an explicit Le made the preflight pass end to end.
- Remaining implementations include TRNG with a vetted physical entropy interface and XDH with the target named-parameter key ABI. The legacy RSA cipher encodings and wider AES MAC vectors are requested in `.agent/EXT_REQUESTS.md`. The user removed the 192- and 256-bit block CBC/ECB Cipher IDs from scope. Existing 128-bit AES block and MAC IDs accept 192- and 256-bit AES keys.
- MessageDigest streaming physical validation is pending. The ARM max_crypto build links, but the pinned CHOUPI Rust OS emits warnings and the warning-free flash gate applies. Once clear, identify a free board and run the full JCVM cycle, then validate the 5 KiB and multipart digest probes with per-APDU flash telemetry. See step `20261121-1119-digest-streaming`.
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- OOM T9 fix committed (`20261120-0419-oom-small-handles-reserve`); the live run is its end-to-end check.
- MEM-01 T7 **passed** 2026-11-20 (VPCD, RP2350 overlay, `a1639b68`): `artifacts/results/jcalgtest-support-vpcd-20261120T062332Z/`. T8 acceptance (options 1 and 2 on wiped tokens 07E7 and F42D) is next after the live physical run; VPCD readers `Virtual PCD 00 00/01` exist since 2026-11-20 (user install). Physical option 1 on 07E7 was resumed after a pcscd restart; its reader name is now `... (07E74C5D7A038DEC) 01 00`, logs `client-resume.log`/`serial-resume.log`.
- F42D is visible in PC/SC (`... (F42DD9D07D6D2B07) 00 00`); usable as the second token for T8.
- Priorities and the state assessment are at the top of `TODO.md`.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
