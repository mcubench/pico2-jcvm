# External validation requests

## WIN-MAXCRYPTO-CAP185-621BA60C — JCAlgTest 1.8.5 CAP performance run, one-block script

Created 2026-11-18 21:35 CET. This replaces `WIN-MAXCRYPTO-FIXES-A418AFEF` (not returned). The firmware
source is unchanged from a418afef: it has the same fixes for the EC_FP_128/160 leak, the 64-record /
3 s deletion pass, the digest tag index and RSA workspace reuse. Only the CPLC id differs. A run
already in progress on a418afef may be finished and returned; it stays valid evidence for that image.

What is new in the package:
- The CAP is the user-built JCAlgTest **1.8.5/jc305** (`applet.cap`, SHA-256
  `fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`). Its RSA NOPAD setup input
  no longer exceeds the modulus, so the random `F105` on RSA NOPAD should be gone.
- `README.md` has one PowerShell block (also `run-test.ps1`). It does the following:
  - flashes through the BOOTSEL volume labelled `rp2350`;
  - waits for `Pico2 JCVM CCID 0`, reads the chip ID and COM port, and starts the serial capture;
  - installs the CAP and runs the client;
  - stops the capture when the client exits and packs `results\<mode>_<timestamp>.zip`.

  Modes:
  - `$mode = 'all'` is `2 -> PERFORMANCE TEST -> 1` (5–8 h). It runs purely from the command line:
    `-op ALG_PERFORMANCE_STATIC -cardname … -fresh -outpath …`.
  - `$mode = 'symmetric'` is `2 -> 2` (1–3 h). The 1.8.5 client has no switch for this, so the
    block answers both menu prompts once each appears in the client log.

Normal ARM `max_crypto` image from commit `621ba60c` (Arm GNU 14.2.1): object deletion ON, all debug
and trace switches OFF, 0 ms startup wait, erased 1 MiB storage region, no preinstalled applet. The
package README text includes one later packager fix that only escapes `PATH` in Markdown.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-621ba60c.uf2)
- UF2 SHA-256: `616b2d7b364ee18cf47e2b11aafc2003cbb1dffafa1a346fb9edd584155824f1`; size 2,939,392 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-621ba60c.zip)
- ZIP SHA-256: `49409c218aae7f3a43ffefa91c180432af3c78af16203c6c69554c4a52fe4052`; size 18,224,413 bytes.
- CPLC source identifier `0x621BA60C4E29EFA2`; linked end `0x10066b40`, firmware-window headroom
  627,904 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: `./tools/build arm --profile max_crypto` exit 0 with 0 warning lines;
  `PACKAGE:PASS` (includes `fixture-path --verify`). The UF2 was not flashed here, and the
  PowerShell block has not been run here: no PowerShell is available and it has not been tried on
  Windows. The 1.8.5 CAP was checked only in the emulator (install, GETVERSION `1.8.5_jc305`).

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first, if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip`: client log/CSV, `serial.log`, GPPro output and `run-identity.txt`;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Do the RSA NOPAD Cipher methods (all key lengths, public and private) now complete without
  `F105`?
- Does `all` pass the EC_FP_128/160 Signature, KeyPair and KeyAgreement methods with no
  `*** PANIC ***` and no reader loss, and reach the end?
- Is the first of the five baseline values close to the other four?
- Is every `OBJECT_DELETION … pending=0` with `tag_index_refused=0`? Is the `HEADROOM:` verdict at
  the end of `serial-capture-console.txt` PASS?
- Does the RSA_CRT_PRIVATE 1280 NOPAD transport loss (PCSC 0x1f) recur?

## HOST-VPCD-PCSC-01 — Restore the host VPCD PC/SC reader

Created 2026-11-18 17:38 CET. This is the current blocker for MEM-01 T7's unchanged-client VPCD run; the full evidence and prior checks are in `.agent/QKB.md` under “PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable.”

Please restore or install a VPCD PC/SC service on this host so the PC/VPCD JCVM emulator is published through PC/SC as exactly `Virtual PCD 00 00`. The emulator wrapper cannot publish this reader on its own, and the current host has no `vpcd`/`vicc` executable in PATH or `/usr/lib/pcsc`.

After restoring it, provide:
- `./tools/list-readers` output showing `Virtual PCD 00 00` (use the required host-access path if this performs PC/SC access);
- the service/driver name and version, plus any configuration or startup command needed to keep it available;
- confirmation that a SmartcardIO client can open that reader.

Do not restart `pcscd` while the physical option-1 campaign is live; its client is currently running on token `07E74C5D7A038DEC`. Once a safe maintenance window is available, notify the agent so it can check reader publication and resume T7 using the pinned 1.8.5 client.
