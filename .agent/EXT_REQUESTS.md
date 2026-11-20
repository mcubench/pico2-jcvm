# External validation requests

## WIN-MAXCRYPTO-OOMFIX-981CDE78 — JCAlgTest 1.8.5 option 1 and 2 with the out-of-memory fix

Created 2026-11-20 09:28 CET. This replaces `WIN-MAXCRYPTO-FLASH-A266B836` (on that image option 1
panics with `Out of memory` at RSA-3072 CRT). A run already in progress on a266b836 may be finished
and returned; option-2 (`symmetric`) results from it remain valid evidence for that image.

Same package layout, CAP (1.8.5/jc305, `applet.cap` SHA-256
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`), client and one-block
PowerShell run (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as before.
New since a266b836 (records in `progress.md`, 2026-11-20):
- Each live Java object costs less RAM (array handle 84 → 52 B, instance 104 → 56 B; measured
  176 → 144 B and 153 → 105 B of heap per object). At the old failure point (405 arrays /
  144 instances live) free heap is ~61 KB instead of ~11 KB.
- New Java objects are refused with `SystemException.NO_RESOURCE` when less than 16 KB of heap
  would remain, so native crypto never hits the SDK `Out of memory` panic.
- Deletion passes under memory pressure keep using the fast tag index (`tag_index_refused=0`).
- Everything from a266b836 (flash journal, deletion skip, native OwnerPIN) is included.

Normal ARM `max_crypto` image from commit `981cde78` (firmware source identical to `a1639b68`;
Arm GNU 14.2.1): object deletion ON, all debug and trace switches OFF, erased storage region, no
preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-981cde78.uf2)
- UF2 SHA-256: `d175b84e0dfd43bb3f9f332c749aa019b18e4bb0513b6b361221c58113a96a28`; size 2,956,288 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-981cde78.zip)
- ZIP SHA-256: `5d9dd84765b04cae633d757e768e5e9dc5d91534006f57ff148cc74855206507`; size 18,232,734 bytes.
- CPLC source identifier `0x981CDE78CAFC2ED0`; linked end `0x10068c6c`, firmware-window headroom
  619,412 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: build exit 0, no warnings; `PACKAGE:PASS`. The exact UF2 was flashed to
  F42DD9D07D6D2B07 and boots (`TEST:PASS choupi_runtime`). Same firmware source on 07E7:
  MEM-01 T7 VPCD pass and a physical option-1 rerun past RSA-3072 CRT without faults (see
  `progress.md`). A PC/SC install smoke test of this exact UF2 follows when the live 07E7 run ends.
  The PowerShell block has not been run (no PowerShell here).

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first, if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip`: client log/CSV, `serial.log`, GPPro output and `run-identity.txt`;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Does `all` reach the end with no `*** PANIC ***`, `FAULT` or reader loss, in particular through
  RSA-3072/4096 CRT?
- Does the serial log show any `NO_RESOURCE`, or `OBJECT_DELETION … pending=1` / `tag_index_refused=1`?
- Is the `HEADROOM:` verdict at the end of `serial-capture-console.txt` PASS? How many
  `FS_APDU … synced=1` lines are there?
