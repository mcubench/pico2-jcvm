# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest commit `3fa22bc8` fixes RP2350 overlay wrapper retention under LTO; preceding `eb79e487` records the replay and PC/SC findings.
- ARM `max_crypto` rebuild passes: normal mode, object deletion ON, debug switches OFF, 36,024 bytes remain in the 1 MiB flash window. UF2 SHA-256 `20493fbb3028058d28ae10a39520fff55672a82a038e717a308e22e32dd39e8b`; provenance `build/arm/build-provenance.txt`.
- Latest `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported 0 devices; elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` returned `list() failed`. No flash was attempted. Refresh both before any hardware work.
- T7 replay failure: on the `max_crypto` overlay, both current and Nov 15 1.8.5 client transcripts return `6F00` instead of `9000` at APDUs 1584/1586/1588/1590. The source logs are different captures (line 4751 vs 13161); the old passing replay used ROM mask hash `184d2794…`, not the current `ab55f462…`. Do not call this a proven same-build regression. CAP handler offset 15578..15698 catches CryptoException reason 3 and rethrows at 15701; the runtime cause remains open. Detailed traces: `artifacts/results/resource01-apdu-replay-vpcd-20261117T142939Z/`, `/144120Z/`, `/144244Z/`.
- The failing APDU uses algorithm 18, `ALG_AES_BLOCK_192_CBC_NOPAD`, which the native VM correctly rejects with `NO_SUCH_ALGORITHM`; captured expected response is reason `03` and `9000`. Same-context native exception handling passes in the JCSystem probe (unsupported digest and 50 CryptoException throws/catches); the issue appears in the JCAlgTest cross-package exception/result path.
- Separate open test failure: `./tools/test-jcsystem-transaction-applet --case 89` returns `6F00` while updating the 32,767-byte persistent array non-atomically inside a transaction. Trace records a 32,793-byte write failing in a 65,536-byte sector after defrag. See `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261117T144708Z.log`.
- Overlay build now links with empty `CMAKE_EXE_LINKER_FLAGS`; current binary hash is `4fd35962baf48a2a783a0305c6e8e19e3bb700b7e0ffda951b93d8c781e68024` and `nm` confirms `__wrap_fs_sync`, `__wrap_fs_erase_prefix`, `__wrap_fs_erase_package_state` are present.

## Next actions
1. Diagnose the JCAlgTest cross-package exception/result path for unsupported Cipher algorithm 18; preserve the correct `NO_SUCH_ALGORITHM` behavior and do not add invalid AES block-size support.
2. Diagnose the separate transaction case-89 persistent array write failure (32,793-byte record in a 65,536-byte sector) against non-atomic arrayCopy semantics.
3. Re-run focused replay on the `max_crypto` overlay after any fix, then restart unchanged-client option 2 from fresh state to its configured pass/timeout when PC/SC works. Physical options 2 and 1 remain acceptance gates after T7.
4. Refresh approved helper and reader list before hardware. No board is currently enumerated, so no full JCVM cycle was run after ARM build.
5. Windows external request and host/token troubleshooting are tracked in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
