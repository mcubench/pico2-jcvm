# External validation requests

## WIN-MAXCRYPTO-BASELINE-5A537513 — JCAlgTest 1.8.5 run with fast baselines and commit journal

Created 2026-11-19 08:20 CET. This replaces `WIN-MAXCRYPTO-CAP185-621BA60C`. A run already in
progress on 621ba60c may be finished and returned; it stays valid evidence for that image.

Same package layout, CAP (1.8.5/jc305, `applet.cap` SHA-256
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`), client and one-block
PowerShell run (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as 621ba60c.
New in the firmware (details: `docs/review_slowbaseline_20261119.md`):
- A deletion pass drains a whole prepare's garbage (cap 512 records), so it no longer spills into
  the first baseline.
- Commands whose persistent changes cancel out (JCAlgTest baselines) skip the flash write; other
  small commits append one 256 B entry to a flash journal instead of rewriting the whole store.
  Baselines and measurements no longer carry ~0.5–9 s of store rewrite.
- EC keys store their components in one record and Signature buffers are created on first use:
  an EC384 Signature prepare leaves 52 dead records instead of 118.
- A deletion pass uses one complete inventory scan per batch.
- Serial gains one `CHOUPI:FS_APDU` line per applet command (VM time, sync time, journal use).

Normal ARM `max_crypto` image from commit `5a537513` (Arm GNU 14.2.1): object deletion ON, all
debug and trace switches OFF, erased storage region, no preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-5a537513.uf2)
- UF2 SHA-256: `77679509ad5d7bdb7124acf1cabe1e37e10ec2e604a2999c872f0c491de12bb0`; size 2,948,096 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-5a537513.zip)
- ZIP SHA-256: `4b70b8d7a1f2c877be2626d2b0ad1be08176c186c227e1285be52ee48eae165d`; size 18,228,829 bytes.
- CPLC source identifier `0x5A537513FC6C709D`; linked end `0x10067c14`, firmware-window headroom
  623,596 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: build exit 0 with 0 warnings; `PACKAGE:PASS`. The exact UF2 was flashed to
  07E74C5D7A038DEC (empty card); the package's GPPro listed it and installed the package CAP; an
  EC384 Signature replay (prepare, 5 × baseline, 5 × measurement, twice) gave baselines 233–236 ms
  and measurements 506–507 ms with every deletion pass `pending=0`; the headroom checker reports
  PASS on that serial log (`artifacts/results/windows-package-smoke-5a537513-20261119T0716Z/`). The PowerShell block has still not been run (no
  PowerShell here).

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first, if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip`: client log/CSV, `serial.log`, GPPro output and `run-identity.txt`;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Are the five baseline values of each method close to each other (no long first baseline), and
  are RSA Cipher baselines now well under a second instead of 9–10 s?
- Is every `OBJECT_DELETION … pending=0` with `tag_index_refused=0`? Is the `HEADROOM:` verdict at
  the end of `serial-capture-console.txt` PASS?
- Does `all` reach the end with no `*** PANIC ***`, `FAULT` or reader loss (including RSA_CRT_PRIVATE
  1280 NOPAD, PCSC 0x1f on earlier images)?
- Do the RSA NOPAD Cipher methods complete without `F105`?

## HOST-VPCD-PCSC-01 — Restore the host VPCD PC/SC reader

Created 2026-11-18 17:38 CET. This is the current blocker for MEM-01 T7's unchanged-client VPCD run; the full evidence and prior checks are in `.agent/QKB.md` under “PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable.”

Please restore or install a VPCD PC/SC service on this host so the PC/VPCD JCVM emulator is published through PC/SC as exactly `Virtual PCD 00 00`. The emulator wrapper cannot publish this reader on its own, and the current host has no `vpcd`/`vicc` executable in PATH or `/usr/lib/pcsc`.

After restoring it, provide:
- `./tools/list-readers` output showing `Virtual PCD 00 00` (use the required host-access path if this performs PC/SC access);
- the service/driver name and version, plus any configuration or startup command needed to keep it available;
- confirmation that a SmartcardIO client can open that reader.

Do not restart `pcscd` while the physical option-1 campaign is live; its client is currently running on token `07E74C5D7A038DEC`. Once a safe maintenance window is available, notify the agent so it can check reader publication and resume T7 using the pinned 1.8.5 client.
