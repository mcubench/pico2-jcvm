# External validation requests

## WIN-SIZE-MAXCRYPTO-4BDED540 — JCAlgTest option 2 on the reduced-size image

Created 2026-11-18 17:52 CET. This replaces `WIN-SIZE-MAXCRYPTO-58B01075` (created 15:24, not
returned), which shipped an older image without the fprintf and Mbed TLS reductions. If a run on
the 58b01075 image is already in progress, please finish it and return it too; it remains valid
evidence for that image.

Normal ARM `max_crypto` image from commit `4bded540` (Arm GNU 14.2.1): object deletion ON, all
debug and trace switches OFF, 0 ms startup wait, erased 1 MiB storage region, no preinstalled
applet. The build reports `state=dirty` only because of uncommitted non-firmware files (other
session's HANDOFF); firmware sources equal commit `e17d25dc`.

Runtime changes compared with the earlier 853543d6 image (`progress.md` Step-Ids
`20261118-1500-romfs-trim`, `20261118-1503-cap-load-session-bss`, `20261118-1719-fprintf-wrap`,
`20261118-1727-mbedtls-user-config`; overview in `docs/review_firmware_size_20261118.md`):
- Only the 35,045-byte ROM filesystem (Java Card API + GP packages) is embedded instead of the
  512 KiB host ROM-mask dump. Record bytes unchanged.
- The 66 KB CAP-load session object (GP LOAD/INSTALL/DELETE) is zero-initialized in RAM instead
  of copied from flash.
- VM `fprintf(stderr, …)` diagnostics, including all `OBJECT_DELETION:*` telemetry, now go
  through the Pico SDK printf instead of newlib's. Text is unchanged except `%p` pointer fields
  (uppercase hex, no `0x`).
- Mbed TLS no longer contains the unused secp k1, Brainpool and Curve25519/448 curves or SHA-3.
  secp192/224/256/384/521r1, explicit-parameter EC, MD5, RIPEMD-160 and all SHA-1/2 HMACs remain.
- Firmware 1,016,636 → 420,396 bytes; firmware-window headroom 31.9 KB → 628,180 bytes.
- Paths to exercise: CAP install (ROM packages, CAP-load session), every EC algorithm and key
  size, HMAC (incl. MD5/RIPEMD-160), and object deletion with its serial telemetry.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-4bded540.uf2)
- UF2 SHA-256: `a0b46b33b48f13ffe9cb48debeddce7bad46fb1ba8911e0fc0ff28618b3fe6e9`; size 2,938,880 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-4bded540.zip)
- ZIP SHA-256: `be4b32f66c30bf78d3a282c8ce48116e487e9bf7c35c54456a0615bcbc7afa73`; size 18,218,734 bytes.
- CPLC source identifier `0x4BDED5401CDE0EC4`; linked end `0x10066a2c`. Built by
  `tools/package-maxcrypto-windows-test`; `BUILD-PROVENANCE.txt` and `SHA256SUMS.txt` are in the ZIP.
- The ZIP also holds the unchanged JCAlgTest 1.8.4/jc305 CAP, the supplied 1.8.5 client
  (SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`), GPPro and
  the serial capture script.
- Validation here (Linux, not acceptance):
  - `./tools/build arm --profile max_crypto` from HEAD `4bded540`: exit 0, 0 warnings,
    `FLASH_LAYOUT:PASS remaining=628180`; `PACKAGE:PASS` (includes `fixture-path --verify`).
  - This exact UF2 was flashed with `tools/flash` to token F42DD9D07D6D2B07 (storage wiped);
    serial showed 18× `TEST:PASS choupi_runtime`, `CHOUPI:STATUS sw=9000`, no FAULT/TEST:FAIL.
  - **Not tested here:** GPPro list/install, any APDU traffic, ECC, HMAC and object deletion.
    F42D is not listed by pcscd, and pcscd cannot be refreshed during a live campaign on another
    token. Step 1 below is therefore the first CAP install on this image.
  - Logs: `artifacts/results/windows-package-smoke-4bded540-20261118T1648Z/`,
    `artifacts/results/flash-size-opt-20261118/`.
  - Not tested on Windows.

Steps:
1. Unzip and follow `README.md`: verify the UF2 hash, flash through BOOTSEL, confirm the card is
   empty (`gp --list` shows only the ISD), install the CAP explicitly with GPPro, and list again.
   If list or install fails, stop and return the GPPro output and serial log.
2. Run option `2 -> PERFORMANCE TEST -> 1 (all algorithms)` from the start, with a new card name.
3. Capture the board's serial output for the whole run with `tools\capture-object-deletion.ps1`
   (115200 baud, never 1200). Set `-Seconds` longer than the run (e.g. 43200); the default is
   3600. Please do not use a terminal program with a limited scrollback.
4. If the reader disappears, note whether the board came back by itself and keep the capture
   running across the reconnect.

Return:
- GPPro output from step 1 (list before install, install, list after install);
- the complete client result directory (log, CSV, `already_measured.list`) and the whole serial log;
- Windows version, reader name and ATR, chip ID, UF2 and client JAR hashes;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Do the empty-card list, CAP install and applet SELECT work on this image?
- Does option 2 reach the end with no reader loss and no `*** PANIC ***`?
- Are all EC (ECDSA, ECDH, key pair) and HMAC results the same as on earlier images, with no new
  `NO_SUCH_ALGORITHM` or failures?
- Do `OBJECT_DELETION:TIMING … total_us=` lines appear and stay well below 15,000,000 (expected
  about 2–3 s late in the run)?

## HOST-VPCD-PCSC-01 — Restore the host VPCD PC/SC reader

Created 2026-11-18 17:38 CET. This is the current blocker for MEM-01 T7's unchanged-client VPCD run; the full evidence and prior checks are in `.agent/QKB.md` under “PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable.”

Please restore or install a VPCD PC/SC service on this host so the PC/VPCD JCVM emulator is published through PC/SC as exactly `Virtual PCD 00 00`. The emulator wrapper cannot publish this reader on its own, and the current host has no `vpcd`/`vicc` executable in PATH or `/usr/lib/pcsc`.

After restoring it, provide:
- `./tools/list-readers` output showing `Virtual PCD 00 00` (use the required host-access path if this performs PC/SC access);
- the service/driver name and version, plus any configuration or startup command needed to keep it available;
- confirmation that a SmartcardIO client can open that reader.

Do not restart `pcscd` while the physical option-1 campaign is live; its client is currently running on token `07E74C5D7A038DEC`. Once a safe maintenance window is available, notify the agent so it can check reader publication and resume T7 using the pinned 1.8.5 client.
