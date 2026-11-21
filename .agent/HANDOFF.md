# Handoff — next steps only

Updated: 2026-11-21 15:17 CET.

## Current state
- **No live campaign** (2026-11-21 12:26). MEM-01 T8 option 1 PASSED on 07E7 (9228c583, 1,901 methods over two segments, `artifacts/results/t8-option1-9228c583-20261120T2006Z/07E74C5D7A038DEC/`). 07E7 still holds that card state; reader `... (07E74C5D7A038DEC) 00 00`.
- F42D is free again (normal max_crypto build of HEAD after 2026-11-21 15:14, restored with `tools/cycle choupi` after a diagnostic fault-injection run; card not wiped, store about 83 KB of 98.3 KB; reader `... (F42DD9D07D6D2B07) 02 00` at last check).
- Firmware at HEAD (CAPIN-05/07 changes after 9228c583) has not been flashed; the live campaign uses 9228c583.
- Integration check passed: JCAlgTest 1.8.5 `ALG_SUPPORT_BASIC` on 07E7, `artifacts/results/integration-support-basic-c44967d9-20261119/07E74C5D7A038DEC/`.

## Next work
- Stopped at user request 2026-11-21 15:37 CET. Open: GitHub Release of `v0.2-rc1` (tag local, push only after user confirmation and Windows results, `WIN-MAXCRYPTO-RC-V0.2-RC1`); CAPIN-07 LOAD-time move (CAPIN-01) and resolveClassref record bound (reverted attempt broke SIO SensitiveResult); remaining P1 backlog in TODO.md.
- 07E7 exhausted its object store during the broad probe, and removal of the temporary probe failed. Development validation switched to EE11. Storage-preserving cycle on the DRBG tree passed (UF2 SHA-256 `7f6ea308a86888580717523cc89faf8070da00fcb1bf66a09274a5bde67706ad`). Use the exact reader `Pico 2 CHOUPI CCID [JCVM CCID] (07E74C5D7A038DEC) 01 00`. Check current processes and F42D ownership before any shared PC/SC refresh.
- Native SHA3, DES MAC4, CFB/XTS, GCM/CCM, CRC16/32, pseudo-random and preseeded DRBG commits landed. The max_crypto simulator suite passed 206 vectors. Physical crypto KAT is still in progress.
- On-card RSA and RSA-CRT 3072 generation passed on 07E7. The two 4096-bit hardware runs are deferred at user request. See `artifacts/logs/test-rsa3072-hardware.log`. The four-byte physical preflight APDUs initially failed with 6F00. Supplying an explicit Le made the preflight pass end to end.
- Both TRNG factories passed short physical checks on EE11. XDH still needs the named-parameter key ABI wired into the runtime. The user removed the 192- and 256-bit block CBC/ECB Cipher IDs the ISO14888/ISO9796 RSA Cipher IDs, and the AES MAC 192/256 NOPAD IDs from scope. Existing 128-bit AES block and MAC IDs accept 192- and 256-bit AES keys.
- MessageDigest streaming physical validation is pending. The ARM max_crypto build links, but the pinned CHOUPI Rust OS emits warnings and the warning-free flash gate applies. Once clear, identify a free board and run the full JCVM cycle, then validate the 5 KiB and multipart digest probes with per-APDU flash telemetry. See step `20261121-1119-digest-streaming`.
- Waiting on `WIN-MAXCRYPTO-FLASH-A266B836` (`.agent/EXT_REQUESTS.md`).
- OOM T9 fix committed (`20261120-0419-oom-small-handles-reserve`); the live run is its end-to-end check.
- MEM-01 T7 **passed** 2026-11-20 (VPCD, RP2350 overlay, `a1639b68`): `artifacts/results/jcalgtest-support-vpcd-20261120T062332Z/`. T8 acceptance (options 1 and 2 on wiped tokens 07E7 and F42D) is next after the live physical run; VPCD readers `Virtual PCD 00 00/01` exist since 2026-11-20 (user install). Physical option 1 on 07E7 was resumed after a pcscd restart; its reader name is now `... (07E74C5D7A038DEC) 01 00`, logs `client-resume.log`/`serial-resume.log`.
- F42D is visible in PC/SC (`... (F42DD9D07D6D2B07) 00 00`); usable as the second token for T8.
- Priorities and the state assessment are at the top of `TODO.md`.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the pre-existing root `*_already_measured.list` files.
