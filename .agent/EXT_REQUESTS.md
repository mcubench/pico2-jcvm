# External validation requests

## WIN-MEM01-MAXCRYPTO-DE729D28 — JCAlgTest option 2 on the RSA OOM-fix build

Created 2026-11-17 22:10 CET. Supersedes `WIN-MEM01-MAXCRYPTO-C050A193` (its results
were returned in `artifacts/results/windows-maxcrypto-20261117-c050a193-deletion/` and
ended in `*** PANIC *** Out of memory` at RSA-1984 PKCS1 `Cipher_doFinal()` ×50) and
`WIN-RSA1984-X50-SERIAL-20261115` (that diagnostic targeted the unfixed image).

Normal ARM `max_crypto` image from commit `de729d28` (Arm GNU 14.2.1): object deletion
ON, all debug and trace switches OFF, 0 ms startup wait, erased 1 MiB storage region, no
preinstalled applet. The build reports `state=dirty` only because of three untracked
non-source files in the working copy; no tracked file differs from `de729d28`.

Changes since c050a193 (analysis: `docs/review_rsa1984_oom_opus55_20261117.md`):
- RSA `Cipher.doFinal` no longer allocates a JC heap array per call (the ×50 measure held
  51 dead arrays, about 36 KB, inside one APDU).
- Persistent arrays drop their RAM element copy once committed, and store their tag at its
  exact length (about 212 B less RAM per live array).
- Applet-requested object deletion compacts filesystem tombstones once 256 accumulate (the
  c050a193 card had grown to about 5,100 overlay files).
- Also included since c050a193: CCID time extensions during RSA exponentiation.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-de729d28.uf2)
- UF2 SHA-256: `f40bb1b56c9603456a1c97c86b07723feb0b9290f0ed4046e0adfdac09514b3b`; size 4,123,648 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-de729d28.zip)
- ZIP SHA-256: `8c162d009b58875779156f3417c791a1a52990907964c3de0fd8fb22cad298ad`; size about 18 MB.
- CPLC source identifier `0xDE729D2865DCCADB`; linked end `0x100f745c` (35,748 bytes of
  firmware window remain). Built by `tools/package-maxcrypto-windows-test`;
  `BUILD-PROVENANCE.txt` and `SHA256SUMS.txt` are in the ZIP.
- The ZIP also holds the unchanged JCAlgTest 1.8.4/jc305 CAP, the supplied 1.8.5 client
  (SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`), GPPro and
  the serial capture script.
- Validation here (Linux, not acceptance): this exact UF2 was flashed with `tools/flash`
  to token 07E74C5D7A038DEC. GPPro listed an empty card (ISD only), installed the unchanged
  CAP (applet SELECTABLE), and `tools/test-longop01-rsa1984-physical` passed all 18 APDUs.
  This included RSA-1984 PKCS1 ×50 (48.0 s, transient RAM 117,224 → minimum 112,056 B) and
  RSA-2048 PKCS1 ×50, with no PANIC/FAULT
  (`artifacts/results/windows-package-smoke-de729d28-20261117T210324Z/07E74C5D7A038DEC/`,
  `artifacts/results/longop01-rsa1984-physical-20261117T210419Z/07E74C5D7A038DEC/`).
  Not tested on Windows; no full option-2 run yet.

Steps: unzip, follow `README.md` (verify the UF2 hash, flash through BOOTSEL, confirm the
card is empty, install the CAP explicitly with GPPro), then run option `2 -> PERFORMANCE
TEST` from the start with a new card name. Capture the board's serial output for the whole
run with `tools\capture-object-deletion.ps1` (115200 baud, never 1200). If a second board
is available, `1 -> All algorithms` on it is welcome; keep its results and serial log
separate.

Return: the complete client result directory (log, CSV, `already_measured.list`), the whole
serial log, Windows version, reader name and ATR, chip ID, UF2 and client JAR hashes, and any
USB/PC/SC errors, resets, stalls or timeouts with their times. Key questions:
- Does the run pass RSA-1984 PKCS1 `Cipher_doFinal()` ×50 and reach the end of option 2
  with no `*** PANIC ***`?
- Do the `OBJECT_DELETION:PRE_CLEANUP files=` values stay bounded (hundreds, not
  thousands), with `OBJECT_DELETION:LOW_WATERMARK_COMPACT … tombstones=` lines appearing?


## HOST-VPCD-PCSC-01 — Restore the local Virtual PCD PC/SC reader

Created 2026-11-18 00:12 CET. The unchanged-client MEM-01 T7 VPCD option-2
acceptance cannot start because the local PC/SC inventory lists only physical
Pico 2 readers and does not publish `Virtual PCD 00 00`. The VPCD test wrapper
stopped before client startup. The host has no VPCD driver file under
`/usr/lib/pcsc` and no `vpcd` or `vicc` executable in PATH.

Please restore or install the host's Virtual PCD PC/SC driver/service without
interrupting physical card campaigns, then report the setup command/package and
confirm that `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 15` lists
`Virtual PCD 00 00`. The emulator endpoint expected by this repo is
`127.0.0.1:35963`. No changes to firmware, physical flash, or security settings
are requested.

Evidence: `artifacts/results/jcalgtest-support-vpcd-20261117T230259Z/reader-inventory-1.log`
through `reader-inventory-3.log`; runner exited before connecting the client.
