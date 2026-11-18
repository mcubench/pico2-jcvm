# External validation requests

## WIN-SIZE-MAXCRYPTO-58B01075 — JCAlgTest option 2 on the reduced-size image

Created 2026-11-18 15:24 CET. This replaces `WIN-MEM01-MAXCRYPTO-853543D6`, which had not been
returned. This image contains every runtime change of 853543d6 (faster object-deletion pass,
watchdog feeding and CCID time extensions during long deletion loops, heap GC only on B0E2),
plus two flash-size fixes. If a Windows run on 853543d6 is already in progress, please finish
it and return it too; it remains valid evidence for that image.

Normal ARM `max_crypto` image from commit `58b01075` (Arm GNU 14.2.1): object deletion ON, all
debug and trace switches OFF, 0 ms startup wait, erased 1 MiB storage region, no preinstalled
applet. The build reports `state=dirty` only because of untracked/uncommitted non-firmware files
(other session's HANDOFF, packaging script edit); firmware sources equal commit `7adbe5cc`.

Changes since 853543d6 (`progress.md` Step-Ids `20261118-1500-romfs-trim`,
`20261118-1503-cap-load-session-bss`; overview in `docs/review_firmware_size_20261118.md`):
- The firmware now embeds only the 35,045-byte ROM filesystem (Java Card API + GP packages)
  instead of the 512 KiB host ROM-mask dump; lookups start at offset 0. Record bytes unchanged.
- The 66 KB CAP-load session object is zero-initialized in RAM (`.bss`) instead of carrying a
  flash copy. Values and RAM use unchanged.
- Firmware 1,016,636 → 460,868 bytes; firmware-window headroom 31.9 KB → 587,708 bytes.
- These fixes change where the ROM API packages are read from and how the GP LOAD/INSTALL/DELETE
  session object is initialized. Those paths are what this run should exercise.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-58b01075.uf2)
- UF2 SHA-256: `de7e858a726f4752691668dc9cc90ce5c3f1a963f34833a082b7e6a082fa1e51`; size 3,019,776 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-58b01075.zip)
- ZIP SHA-256: `b4cf2d09ff3b5cf7855f708615dd15dba834086aed18dcf12c6bb76bc8e79e7a`; size 18,252,291 bytes.
- CPLC source identifier `0x58B01075F6632556`; linked end `0x10070844` (587,708 bytes of firmware
  window remain). Built by `tools/package-maxcrypto-windows-test`; `BUILD-PROVENANCE.txt` and
  `SHA256SUMS.txt` are in the ZIP.
- The ZIP also holds the unchanged JCAlgTest 1.8.4/jc305 CAP, the supplied 1.8.5 client
  (SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`), GPPro and
  the serial capture script.
- Validation here (Linux, not acceptance):
  - `./tools/build arm --profile max_crypto` from HEAD `58b01075`: exit 0, 0 warnings,
    `FLASH_LAYOUT:PASS remaining=587708`.
  - This exact UF2 was flashed with `tools/flash` to token F42DD9D07D6D2B07 (storage wiped);
    serial showed 18× `TEST:PASS choupi_runtime`, `CHOUPI:STATUS sw=9000`, no FAULT/TEST:FAIL.
    The boot self-test runs the ROM starter package from the trimmed ROM filesystem.
  - **Not tested here:** GPPro list/install of the CAP and any APDU traffic. F42D is not listed
    by pcscd, and pcscd could not be refreshed during a live campaign on another token.
    Step 1 below is therefore the first CAP install on this image.
  - Logs: `artifacts/results/windows-package-smoke-58b01075-20261118T1424Z/`,
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
- Do the `OBJECT_DELETION:TIMING … total_us=` values stay well below 15,000,000 (expected
  about 2–3 s late in the run)?

## HOST-VPCD-PCSC-01 — Restore the host VPCD PC/SC reader

Created 2026-11-18 17:38 CET. This is the current blocker for MEM-01 T7's unchanged-client VPCD run; the full evidence and prior checks are in `.agent/QKB.md` under “PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable.”

Please restore or install a VPCD PC/SC service on this host so the PC/VPCD JCVM emulator is published through PC/SC as exactly `Virtual PCD 00 00`. The emulator wrapper cannot publish this reader on its own, and the current host has no `vpcd`/`vicc` executable in PATH or `/usr/lib/pcsc`.

After restoring it, provide:
- `./tools/list-readers` output showing `Virtual PCD 00 00` (use the required host-access path if this performs PC/SC access);
- the service/driver name and version, plus any configuration or startup command needed to keep it available;
- confirmation that a SmartcardIO client can open that reader.

Do not restart `pcscd` while the physical option-1 campaign is live; its client is currently running on token `07E74C5D7A038DEC`. Once a safe maintenance window is available, notify the agent so it can check reader publication and resume T7 using the pinned 1.8.5 client.
