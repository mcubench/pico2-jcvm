# External validation requests

## WIN-MAXCRYPTO-FLASH-A266B836 — JCAlgTest 1.8.5 run with minimal flash writes and native OwnerPIN

Created 2026-11-19 23:15 CET. This replaces `WIN-MAXCRYPTO-BASELINE-5A537513`. A run already in
progress on 5a537513 may be finished and returned; it stays valid evidence for that image.

Same package layout, CAP (1.8.5/jc305, `applet.cap` SHA-256
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`), client and one-block
PowerShell run (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as before.
New since 5a537513 (records in `progress.md`, 2026-11-19 evening):
- Commits of any size go to the flash journal as one atomic group; the journal fills the rest of
  the snapshot slot and its sectors are erased in untimed `B0E2` commands. Full 40–64 KB snapshot
  rewrites are now rare (2 in a 15-minute support run), and timed commands never erase flash.
- A deletion request is answered without a heap trace when nothing can have become unreachable
  since the last complete pass (`OBJECT_DELETION:UNCHANGED`): most `B0E2` take ~60 ms instead of
  ~190 ms.
- Compaction after a deletion pass no longer forces a full snapshot.
- OwnerPIN: the try counter is written durably outside any transaction before the PIN comparison;
  the validated flag lives in RAM and clears on reset; constant-time comparison.
- Serial `CHOUPI:FS_APDU` gains `journal_erase_bytes` (last field).

Normal ARM `max_crypto` image from commit `a266b836` (Arm GNU 14.2.1): object deletion ON, all
debug and trace switches OFF, erased storage region, no preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-a266b836.uf2)
- UF2 SHA-256: `63abbf3d37dd9a86c37324ec72cedd06fee10204c84e59b646d793b726a99f8b`; size 2,956,288 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-a266b836.zip)
- ZIP SHA-256: `e92205e3f8b5e146ce3920198ee1d2c02a3c08472793dce5ceef033cd9ff4213`; size 18,232,580 bytes.
- CPLC source identifier `0xA266B8362C75CD36`; linked end `0x10068c08`, firmware-window headroom
  619,512 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: build exit 0, no C/C++ warnings; `PACKAGE:PASS`; README block byte-identical to
  `run-test.ps1`. The exact UF2 was flashed to 07E74C5D7A038DEC (empty card); the package's GPPro
  listed it and installed the package CAP; an EC384 Signature replay (prepare, 5 × baseline,
  5 × measurement, twice) gave baselines 242–249 ms, measurements 522–526 ms, `B0E2` 62 ms after
  the first two passes, every pass `pending=0`, no full snapshot, HEADROOM:PASS
  (`artifacts/results/windows-package-smoke-a266b836-20261119T2207Z/`). The PowerShell block has not
  been run (no PowerShell here).

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first, if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip`: client log/CSV, `serial.log`, GPPro output and `run-identity.txt`;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Are the five baseline values of each method close to each other, and are RSA Cipher baselines
  well under a second?
- Is every `OBJECT_DELETION … pending=0` with `tag_index_refused=0`? Is the `HEADROOM:` verdict at
  the end of `serial-capture-console.txt` PASS? How many `FS_APDU … synced=1` lines are there?
- Does `all` reach the end with no `*** PANIC ***`, `FAULT` or reader loss?
- Do the RSA NOPAD Cipher methods complete without `F105`?

## HOST-VPCD-PCSC-01 — Restore the host VPCD PC/SC reader

Created 2026-11-18 17:38 CET. This is the current blocker for MEM-01 T7's unchanged-client VPCD run; the full evidence and prior checks are in `.agent/QKB.md` under “PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable.”

Please restore or install a VPCD PC/SC service on this host so the PC/VPCD JCVM emulator is published through PC/SC as exactly `Virtual PCD 00 00`. The emulator wrapper cannot publish this reader on its own, and the current host has no `vpcd`/`vicc` executable in PATH or `/usr/lib/pcsc`.

After restoring it, provide:
- `./tools/list-readers` output showing `Virtual PCD 00 00` (use the required host-access path if this performs PC/SC access);
- the service/driver name and version, plus any configuration or startup command needed to keep it available;
- confirmation that a SmartcardIO client can open that reader.

Do not restart `pcscd` while the physical option-1 campaign is live; its client is currently running on token `07E74C5D7A038DEC`. Once a safe maintenance window is available, notify the agent so it can check reader publication and resume T7 using the pinned 1.8.5 client.
