# Handoff — next steps only

Updated: 2026-11-21 12:30 CET.

## Current state
- **No live campaign** (2026-11-21 12:26). MEM-01 T8 option 1 PASSED on 07E7 (9228c583, 1,901 methods over two segments, `artifacts/results/t8-option1-9228c583-20261120T2006Z/07E74C5D7A038DEC/`). 07E7 still holds that card state; reader `... (07E74C5D7A038DEC) 00 00`.
- **F42D in use (claude-opus-5-5, from 2026-11-21 11:50 CET):** reproduced WIN-D7E90A18-6F00 on the d7e90a18 image with the new client (RSA block replay died at RSA-CRT private 3072 OAEP during object deletion, like the 07E7 OOM). F42D was cycled to HEAD `a9ec9e04` (normal max_crypto, storage preserved; CYCLE:PASS twice) but is absent from PC/SC; a pcscd restart is needed and allowed only after the 07E7 campaign ends. Then rerun the replay (`artifacts/results/repro-rsablock-head-a9ec9e04-*/`, skip list `pico2_maxcrypto_head_F42DD9D07D6D2B07_repro_rsablock_newclient____…list`) and the RSA-4096 probe APDUs. Do not use F42D until this note is removed.
- Firmware at HEAD (CAPIN-05/07 changes after 9228c583) has not been flashed; the live campaign uses 9228c583.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- EE11 is the development token for the additional crypto algorithms. Full JCVM cycle passed on the AES-CFB working tree (UF2 SHA-256 `e317c24635e2f29d835de8ad5d18287fcc58da4a4747b17737651cf798e66ae9`). Raw CCID could not open EE11 after reboot. The subsequent F42D RSA replay is live (confirmed 12:29 CET), so leave shared PC/SC and its reader list alone until that run ends.
- Remaining requested crypto work: AES-GCM and AES-CCM native AEADCipher, RandomData pseudo/TRNG/preseeded DRBG native behavior, and XDH with Java Card named-curve key compatibility. Deprecated RSA cipher IDs and wider-block AES IDs need matching legacy semantics, vetted backends and vectors. The existing AES 128-bit block CBC/ECB IDs already accept 192- and 256-bit keys.
- RSA/RSA-CRT 4096 physical validation is pending the same warning-free ARM build gate. On a free identified board, cycle the normal max_crypto JCVM and run the two 4096 cases from the asymmetric probe. Use one short APDU per mode (801A060000 and 801A070000, expect 019000).
- MessageDigest streaming physical validation is pending. The ARM max_crypto build links, but the pinned CHOUPI Rust OS emits warnings and the warning-free flash gate applies. Once clear, identify a free board and run the full JCVM cycle, then validate the 5 KiB and multipart digest probes with per-APDU flash telemetry. See step `20261121-1119-digest-streaming`.
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- OOM T9 fix committed (`20261120-0419-oom-small-handles-reserve`); the live run is its end-to-end check.
- MEM-01 T7 **passed** 2026-11-20 (VPCD, RP2350 overlay, `a1639b68`): `artifacts/results/jcalgtest-support-vpcd-20261120T062332Z/`. T8 acceptance (options 1 and 2 on wiped tokens 07E7 and F42D) is next after the live physical run; VPCD readers `Virtual PCD 00 00/01` exist since 2026-11-20 (user install). Physical option 1 on 07E7 was resumed after a pcscd restart; its reader name is now `... (07E74C5D7A038DEC) 01 00`, logs `client-resume.log`/`serial-resume.log`.
- F42D is visible in PC/SC (`... (F42DD9D07D6D2B07) 00 00`); usable as the second token for T8.
- Priorities and the state assessment are at the top of `TODO.md`.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
